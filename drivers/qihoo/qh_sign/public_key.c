/* In-software asymmetric public-key crypto subtype
 *
 * See Documentation/crypto/asymmetric-keys.txt
 *
 * Copyright (C) 2012 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#define pr_fmt(fmt) "PKEY: "fmt
#include <linux/module.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include "asy_public_key.h"

MODULE_LICENSE("GPL");

const char *const qh_pkey_algo_name[PKEY_ALGO__LAST] = {
	[PKEY_ALGO_DSA]		= "DSA",
	[PKEY_ALGO_RSA]		= "RSA",
};

const struct public_key_algorithm *qh_pkey_algo[PKEY_ALGO__LAST] = {
#if defined(CONFIG_PUBLIC_KEY_ALGO_RSA) || \
	defined(CONFIG_PUBLIC_KEY_ALGO_RSA_MODULE)
	[PKEY_ALGO_RSA]		= &_RSA_public_key_algorithm,
#endif
};

const char *const qh_pkey_id_type_name[PKEY_ID_TYPE__LAST] = {
	[PKEY_ID_PGP]		= "PGP",
	[PKEY_ID_X509]		= "X509",
};

/*
 * Verify a signature using a public key.
 */
int qh_public_key_verify_signature(const struct public_key *pk,
				const struct public_key_signature *sig)
{
	const struct public_key_algorithm *algo;

	BUG_ON(!pk);
	BUG_ON(!pk->mpi[0]);
	BUG_ON(!pk->mpi[1]);
	BUG_ON(!sig);
	BUG_ON(!sig->digest);
	BUG_ON(!sig->mpi[0]);

	algo = pk->algo;
	if (!algo) {
		if (pk->pkey_algo >= PKEY_ALGO__LAST)
			return -ENOPKG;
		algo = qh_pkey_algo[pk->pkey_algo];
		if (!algo)
			return -ENOPKG;
	}

	if (!algo->verify_signature)
		return -ENOTSUPP;

	if (sig->nr_mpi != algo->n_sig_mpi) {
		pr_debug("Signature has %u MPI not %u\n",
			 sig->nr_mpi, algo->n_sig_mpi);
		return -EINVAL;
	}

	return algo->verify_signature(pk, sig);
}
