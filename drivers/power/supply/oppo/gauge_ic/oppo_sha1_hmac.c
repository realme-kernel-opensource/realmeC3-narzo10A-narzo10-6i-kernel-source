/************************************************************************************
** File:  \\192.168.144.3\Linux_Share\12015\ics2\development\mediatek\custom\oppo77_12015\kernel\battery\battery
** VENDOR_EDIT
** Copyright (C), 2008-2012, OPPO Mobile Comm Corp., Ltd
**
** Description:
**          for dc-dc sn111008 charg
**
** Version: 1.0
** Date created: 21:03:46, 05/04/2012
** Author: Fanhong.Kong@ProDrv.CHG
**
** --------------------------- Revision History: ------------------------------------------------------------
* <version>           <date>                <author>                             <desc>
* Revision 1.0       2018-04-14        Fanhong.Kong@ProDrv.CHG       Upgrade for SVOOC
************************************************************************************************************/

#include "oppo_sha1_hmac.h"

UINT8  Message[RANDMESGNUMBYTES];           // Random message
UINT8  Key[SECRETKEYNUMBYTES];              // Secret key
UINT32 Ws[80];			            // Global Work schedule variable
UINT32 A;
UINT32 B;
UINT32 C1;
UINT32 D;
UINT32 E;
UINT32 H[5];
UINT32 Random[5]; // 16 bytes random message for bq26100 to use in SHA1/HMAC
UINT32 Digest_32[5]; // Result of SHA1/HMAC obtained by MCU is contained here

//*****************************************************************************
//  unsigned long Rotl(unsigned long x, int n)
//							
//  Description : This procedure is a rotate left n spaces of 32-bit word x.
//  Arguments :   x - word to be rotated
//	          n - amount of spaces to rotated to the left								  
//  Returns: Result of 32-bit word rotated n times
//*****************************************************************************
UINT32 Rotl(UINT32 x, int n)
{
  return ( (x<<n) | (x>>(32-n)) );
}	

//*****************************************************************************
// unsigned long W(int t)
//
// Description : Determines the work schedule for W(16) through W(79)
// Arguments : t - index of work schedule 16 through 79
// Global Variables : Ws[]
// Returns : Work schedule value with index t
//*****************************************************************************
UINT32 W(int t)
{
  return (Rotl(Ws[t-3] ^ Ws[t-8] ^ Ws[t-14] ^ Ws[t-16], 1));
}	

//*****************************************************************************
// unsigned long K(int t)
//
// Description : Selects one of the K values depending on the index t
// Arguments : t - index
// Returns : One of the 4 K values
//*****************************************************************************
UINT32 K(int t)
{
  if (t <= 19)
    return 0x5a827999;
  else if ( (t >= 20) && (t <= 39) )
    return 0x6ed9eba1;
  else if ( (t >= 40) && (t <= 59) )
    return 0x8f1bbcdc;
  else if ( (t >= 60) && (t <= 79) )
    return 0xca62c1d6;
  else
    return 0;		                    // Invalid value, not expected
}
	
//*****************************************************************************
// unsigned long f(unsigned long x, unsigned long y, unsigned long z, int t)
//
// Description : This procedure selects the ft(b,c,d) function based
//               on the SLUA389 and FIPS 180-2 documents
// Arguments : x - b as seen in document
//             y - c as seen in document
//             z - d as seed in document
//             t - index
// Returns : Result of ft function
//*****************************************************************************
UINT32 f(UINT32 x, UINT32 y, UINT32 z, int t)
{
  if (t <= 19)
    return ( (x & y) ^ ((~x) & z) );
  else if ( (t >= 20) && (t <= 39) )
    return (x ^ y ^ z);
  else if ( (t >= 40) && (t <= 59) )
    return ( (x & y) ^ (x & z) ^ (y & z) );
  else if ( (t >= 60) && (t <= 79) )
    return (x ^ y ^ z);
  else
    return 0;                               // Invalid value, not expected
}	

//*****************************************************************************
// void SHA1_authenticate(void)
//
// Description : Computes the SHA1/HMAC as required by the bq26100
// Arguments : i - times that SHA1 is executing
//             t - index 0 through 79
//             temp - Used to update working variables
// Global Variables : Random[], Message[], Key[], Ws[], H[], A, B, C1, D, E
// Returns : Result of 32-bit word rotated n times
//*****************************************************************************
void SHA1_authenticate(void)
{
  int i; // Used for doing two times the SHA1 as required by the bq26100
  int t; // Used for the indexes 0 through 79
  UINT32 temp; // Used as the temp variable during the loop in which the
               // working variables A, B, C1, D and E are updated
	
  // The 20 bytes of random message that are given to the bq26100 are arranged
  // in 32-bit words so that the microcontroller can compute the SHA1/HMAC
  Random[0] = (UINT32)(Message[16])*0x00000001 +
              (UINT32)(Message[17])*0x00000100 +
              (UINT32)(Message[18])*0x00010000 +
              (UINT32)(Message[19])*0x01000000;
  Random[1] = (UINT32)(Message[12])*0x00000001 +
              (UINT32)(Message[13])*0x00000100 +
              (UINT32)(Message[14])*0x00010000 +
              (UINT32)(Message[15])*0x01000000;
  Random[2] = (UINT32)(Message[ 8])*0x00000001 +
              (UINT32)(Message[ 9])*0x00000100 +
              (UINT32)(Message[10])*0x00010000 +
              (UINT32)(Message[11])*0x01000000;
  Random[3] = (UINT32)(Message[ 4])*0x00000001 +
              (UINT32)(Message[ 5])*0x00000100 +
              (UINT32)(Message[ 6])*0x00010000 +
              (UINT32)(Message[ 7])*0x01000000;
  Random[4] = (UINT32)(Message[ 0])*0x00000001 +
              (UINT32)(Message[ 1])*0x00000100 +
              (UINT32)(Message[ 2])*0x00010000 +
              (UINT32)(Message[ 3])*0x01000000;
  // The SHA1 is computed two times so that it complies with the bq26100 spec
  for (i = 0; i <= 1; i++)
  {
    // Work Schedule
    // The first four Working schedule variables Ws[0-3], are based on the key
    // that is implied that the bq26100 contains
    Ws[0] = (UINT32)(Key[12])*0x00000001 +
            (UINT32)(Key[13])*0x00000100 +
            (UINT32)(Key[14])*0x00010000 +
            (UINT32)(Key[15])*0x01000000;
    Ws[1] = (UINT32)(Key[ 8])*0x00000001 +
            (UINT32)(Key[ 9])*0x00000100 +
            (UINT32)(Key[10])*0x00010000 +
            (UINT32)(Key[11])*0x01000000;
    Ws[2] = (UINT32)(Key[ 4])*0x00000001 +
            (UINT32)(Key[ 5])*0x00000100 +
            (UINT32)(Key[ 6])*0x00010000 +
            (UINT32)(Key[ 7])*0x01000000;
    Ws[3] = (UINT32)(Key[ 0])*0x00000001 +
            (UINT32)(Key[ 1])*0x00000100 +
            (UINT32)(Key[ 2])*0x00010000 +
            (UINT32)(Key[ 3])*0x01000000;
    // On the first run of the SHA1 the random message is used 		
    if (i == 0)
    {
      Ws[4] = Random[0];
      Ws[5] = Random[1];
      Ws[6] = Random[2];
      Ws[7] = Random[3];
      Ws[8] = Random[4];
    }
    // On the second run of the SHA1, H(Kd || M) is used		
    else
    {
      Ws[4] = H[0];
      Ws[5] = H[1];
      Ws[6] = H[2];
      Ws[7] = H[3];
      Ws[8] = H[4];
    }
    // The Work schedule variables Ws[9-15] remain the same regardless of 
    // which run of the SHA1.  These values are as required by bq26100.
    Ws[9]  = 0x80000000;
    Ws[10] = 0x00000000;
    Ws[11] = 0x00000000;
    Ws[12] = 0x00000000;
    Ws[13] = 0x00000000;
    Ws[14] = 0x00000000;
    Ws[15] = 0x00000120;

    // The Work schedule variables Ws[16-79] are determined by the W(t) func
    for (t = 16; t <= 79; t++)
      Ws[t] = W(t);
    // Working Variables, always start the same regardless of which SHA1 run
    A  = 0x67452301;
    B  = 0xefcdab89;
    C1 = 0x98badcfe;
    D  = 0x10325476;
    E  = 0xc3d2e1f0;
    // Hash reads, always start the same regardless of what SHA1 run
    H[0] = A;
    H[1] = B;
    H[2] = C1;
    H[3] = D;
    H[4] = E;
    // Loop to change working variables A, B, C1, D and E
    // This is defined by FIPS 180-2 document
    for (t = 0; t <= 79; t++)
    {
      temp = Rotl(A,5) + f(B,C1,D,t) + E + K(t) + Ws[t];
      E = D;
      D = C1;
      C1 = Rotl(B,30);
      B = A;
      A = temp;
    }
    // 160-Bit SHA-1 Digest
    H[0] = (A  + H[0]);
    H[1] = (B  + H[1]);
    H[2] = (C1 + H[2]);
    H[3] = (D  + H[3]);
    H[4] = (E  + H[4]);
  }
}
