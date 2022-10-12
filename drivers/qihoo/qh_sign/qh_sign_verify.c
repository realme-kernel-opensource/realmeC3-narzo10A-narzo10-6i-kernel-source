/*
 * qihoo/qh_sign/qh_sign_verify.c for kernel hotfix
 *
 * Copyright (C) 2017 Qihoo, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/klist.h>
#include <crypto/hash.h>
#include <linux/err.h>
#include <linux/linkage.h>

#include "qh_rsa_pub.h"
#include "hash_info.h"
#include "public_key.h"
#include "mpi.h"
#include "asy_public_key.h"

#define QH_SIG_TST	"1d02ad81aeb900d01a4dedf12af13f9c" \
			"9e3da737489e957f42f2a645af421dfb" \
			"5e7e695dbdbd9b76e931b1a876295238" \
			"ad2a62a1c8ff8ad9a63026d30897cfc5" \
			"b242274ff569f06cddb567b8be4c893e" \
			"cbe65145ff4b43d7f7bae661f1858003" \
			"a28bba76053a09858712b9d1b88bc656" \
			"3c29c05350a825f29daa985b7e84cbab"

#define SIG_LEN		128
#define KEY_LEN		31

#define TEST_SIG	0
#define QH_DEBUG	0

#if QH_DEBUG
static void dump_uchar(unsigned char *data, unsigned int len)
{
	int i;
	pr_debug("\n");

	for (i = 0; i < len; i++)
		pr_debug("%02x", data[i]);

	pr_debug("\n");
}

static void dump_mpi(MPI mpi)
{
	int i;
	pr_debug("sizeof(mpi_limb_t).%d, alloced.%d, nlimbs.%d\n",
		sizeof(mpi_limb_t), mpi->alloced, mpi->nlimbs);

	for (i = 0; i < mpi->nlimbs; i++) {
		if (i % 4 == 0)
			pr_debug("\n");
		pr_debug("%08lx ", *(mpi->d + i));
	}
	pr_debug("\n");
}
#else
static void dump_uchar(unsigned char *data, unsigned int len)
{

}

static void dump_mpi(MPI mpi)
{

}
#endif

/*
 * Digest the data contents.
 */
static struct public_key_signature *make_digest(enum hash_algo hash,
						    const void *data,
						    unsigned long dlen)
{
	struct public_key_signature *pks;
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	size_t digest_size, desc_size;
	int ret;

	pr_devel("==>%s()\n", __func__);

	/* Allocate the hashing algorithm we're going to need and find out how
	 * big the hash operational data will be.
	 */
	pr_debug("hash_algo_name is '%s'\n", qh_hash_algo_name[hash]);
	tfm = crypto_alloc_shash(qh_hash_algo_name[hash], 0, 0);
	if (IS_ERR(tfm))
		return (PTR_ERR(tfm) == -ENOENT) ? ERR_PTR(-ENOPKG) : ERR_CAST(tfm);

	desc_size = crypto_shash_descsize(tfm) + sizeof(*desc);
	digest_size = crypto_shash_digestsize(tfm);

	/* We allocate the hash operational data storage on the end of our
	 * context data and the digest output buffer on the end of that.
	 */
	ret = -ENOMEM;
	pks = kzalloc(digest_size + sizeof(*pks) + desc_size, GFP_KERNEL);
	if (!pks)
		goto error_no_pks;

	pks->pkey_hash_algo	= hash;
	pks->digest		= (u8 *)pks + sizeof(*pks) + desc_size;
	pks->digest_size	= digest_size;

	desc = (void *)pks + sizeof(*pks);
	desc->tfm   = tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	ret = crypto_shash_init(desc);
	if (ret < 0)
		goto error;

	ret = crypto_shash_finup(desc, data, dlen, pks->digest);
	if (ret < 0)
		goto error;

	crypto_free_shash(tfm);
	pr_devel("<==%s() = ok\n", __func__);
	return pks;

error:
	kfree(pks);
error_no_pks:
	crypto_free_shash(tfm);
	pr_devel("<==%s() = %d\n", __func__, ret);
	return ERR_PTR(ret);
}

static int string_convert(unsigned char *s, unsigned int len, unsigned char *out)
{
	unsigned int i = 0, j = 0;
	unsigned char tmp = 0;
	int flag = 0, ret = 0;

	/* pr_info("s.%s, len.%u\n", s, len); */
	for (i = 0, j = 0; i < len; i++) {
		if (i % 2 == 0)
			tmp = 0;

		if (s[i] >= 0x30 && s[i] <= 0x39) {
			tmp |= s[i] - 0x30;
			flag = 1;
		} else if (s[i] >= 0x41 && s[i] <= 0x46) {
			tmp |= s[i] - 0x37;
			flag = 1;
		} else if (s[i] >= 0x61 && s[i] <= 0x66) {
			tmp |= s[i] - 0x57;
			flag = 1;
		}

		if (flag == 0) {
			ret = -1;
			pr_err("invalid para.\n");
			break;
		}

		if (i % 2 == 0)
			tmp = tmp << 4;
		else if (i % 2 == 1)
			out[j++] = tmp;
	}

	dump_uchar(out, 128);
	return ret;
}

static int extract_mpi_value(struct public_key_signature *pks, void *data)
{
	MPI mpi;
#if TEST_SIG
	int ret = 0;
	unsigned char data[128] = {0};
	unsigned char *s;
	unsigned int len;

	s = QH_SIG_TST;
	len = strlen(s);
	ret = string_convert(s, len, data);
	if (ret) {
		pr_err("string_convert failed.\n");
		return -EINVAL;
	}
	dump_uchar(data, sizeof(data));
#endif
	dump_uchar(data, 120);
	mpi = qh_mpi_read_raw_data((void *)data, SIG_LEN);
	if (!mpi) {
		pr_err("qh_mpi_read_raw_data failed.\n");
		return -ENOMEM;
	}

	dump_mpi(mpi);
	pks->mpi[0] = mpi;
	pks->nr_mpi = 1;

	return 0;
}

static int init_pkey(struct public_key *pk)
{
	unsigned char data[128] = {0};
	unsigned char *s;
	unsigned int len;
	int ret = 0;

	s = QH_RSA_N;
	len = strlen(s);
	ret = string_convert(s, len, data);
	if (ret) {
		pr_err("%d: string_convert QH_RSA_N failed.\n", __LINE__);
		return -EINVAL;
	}

	pk->rsa.n = qh_mpi_read_raw_data((void *)data, sizeof(data));
	if (!pk->rsa.n) {
		pr_err("%d: qh_mpi_read_raw_data QH_RSA_N failed.\n", __LINE__);
		return -ENOMEM;
	}

	memset(data, 0, sizeof(data));

	s = QH_RSA_E;
	len = strlen(s);
	ret = string_convert(s, len, data);
	if (ret) {
		pr_err("%d: string_convert QH_RSA_E failed.\n", __LINE__);
		ret = -2;
		goto free_rsan;
	}

	pk->rsa.e = qh_mpi_read_raw_data((void *)data, 4);
	if (!pk->rsa.e) {
		pr_err("%d: qh_mpi_read_raw_data QH_RSA_E failed.\n", __LINE__);
		ret = -ENOMEM;
		goto free_rsan;
	}
	memset(data, 0, sizeof(data));

	pk->algo = &_RSA_public_key_algorithm;
	dump_mpi(pk->rsa.n);
	dump_mpi(pk->rsa.e);

	return 0;

free_rsan:
	qh_mpi_free(pk->rsa.n);
	return ret;
}

static int verify_sig(struct public_key *pk, const struct public_key_signature *sig)
{
	int ret = 0;

	ret = init_pkey(pk);
	if (ret != 0) {
		pr_err("init_pkey failed, ret.%d\n", ret);
		return ret;
	}

	ret = qh_public_key_verify_signature(pk, sig);
	if (ret != 0)
		pr_err("Verify failed, ret.%d\n", ret);

	qh_mpi_free(pk->rsa.n);
	qh_mpi_free(pk->rsa.e);

	pr_debug("verify result, ret = %d\n", ret);
	return ret;
}
static void decrypt(void *data, unsigned long len)
{
	int i, j;
	unsigned char key[KEY_LEN] = {0};
	unsigned char *maddr;

	memcpy((void *)key, data, KEY_LEN);

	for (i = 0; i < KEY_LEN; i++)
		pr_debug("%02x ", key[i]);

	pr_debug("\n");
	maddr = (unsigned char *)(data + KEY_LEN);
	len = len - KEY_LEN;
	for (i = 0, j = 0; i < len; i++, j++)
		maddr[i] = maddr[i] ^ key[j % KEY_LEN];

	pr_debug("data.%p, len.%lx, maddr.%p\n", data, len, maddr);
}

/*
 * The data pointer point sig and actual data.
 */
int qh_verify_sig(void *patch, unsigned long plen, void **data, unsigned long *len)
{
	struct public_key_signature *pks;
	struct public_key *pk;
	u8 hash;
	int ret = 0;
	void *_data;
	_data = patch;
	*len = plen;
	if (NULL == patch || NULL == data || NULL == len) {
		pr_err("invalid parameter, patch.%p, data.%p, len.%p", patch, data, len);
		return -EINVAL;
	}

	pr_debug("patch len.%lu\n", plen);
	if (plen <= (SIG_LEN + KEY_LEN)) {
		pr_err("patch len error, patch len.%lu.\n", plen);
		return -EINVAL;
	}

	pk = kzalloc(sizeof(struct public_key), GFP_KERNEL);
	if (!pk) {
		pr_err("pk alloc failed.\n");
		return -ENOMEM;
	}

	hash = HASH_ALGO_SHA1;

	_data = patch;
	_data += SIG_LEN;
	dump_uchar((unsigned char *)patch, 128);
	pks = make_digest(hash, _data, plen - SIG_LEN);
	if (!pks) {
		pr_err("no pks.\n");
		ret = -ENOMEM;
		goto free_pk;
	}

	dump_uchar(pks->digest, 20);

	ret = extract_mpi_value(pks, patch);
	if (ret != 0)
		goto free_pk;

	ret = verify_sig(pk, pks);
	if (ret == 0)
		pr_debug("verify_sig ok.\n");
	else
		pr_debug("verify_sig failed, ret = %d.\n", ret);

	qh_mpi_free(pks->rsa.s);
	kfree(pks);

	*data = _data;
	*len = plen - SIG_LEN;
	decrypt(_data, *len);
	*data = _data + KEY_LEN;
	*len = *len - KEY_LEN;
free_pk:
	kfree(pk);
	return ret;
}

void _qh_verify_sig(void)
{
	pr_debug("hihi");
}

static int __init qh_sign_verify_init(void)
{
#if TEST_SIG
	int ret = 0;
	unsigned char tmp[] = "fheisfihsiaifiehaifei88860960";
	void *patch, *data;
	unsigned long plen, len;

	len = 0;
	data = NULL;
	plen = sizeof(tmp) - 1;
	patch = (void *)tmp;
	pr_debug("zcl before qh_verify_sig\n");
	ret = qh_verify_sig(tmp, plen, &data, &len);
#else
#endif
	pr_debug("Hi, init\n");
	return 0;
}
module_init(qh_sign_verify_init);

static void __exit qh_sign_verify_exit(void)
{
	pr_debug("Bye exit.\n");
}
module_exit(qh_sign_verify_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Verify sign of patch");
