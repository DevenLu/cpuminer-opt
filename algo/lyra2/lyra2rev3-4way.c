#include "lyra2-gate.h"
#include <memory.h>

#if defined (LYRA2REV3_4WAY)	

#include "algo/blake/blake-hash-4way.h"
#include "algo/bmw/bmw-hash-4way.h"
#include "algo/cubehash/cubehash_sse2.h" 

typedef struct {
   blake256_4way_context     blake;
   cubehashParam             cube;
   bmw256_4way_context       bmw;
} lyra2v3_4way_ctx_holder;

static lyra2v3_4way_ctx_holder l2v3_4way_ctx;

bool init_lyra2rev3_4way_ctx()
{
   blake256_4way_init( &l2v3_4way_ctx.blake );
   cubehashInit( &l2v3_4way_ctx.cube, 256, 16, 32 );
   bmw256_4way_init( &l2v3_4way_ctx.bmw );
   return true;
}

void lyra2rev3_4way_hash( void *state, const void *input )
{
   uint32_t vhash[8*4] __attribute__ ((aligned (64)));
   uint32_t hash0[8] __attribute__ ((aligned (64)));
   uint32_t hash1[8] __attribute__ ((aligned (32)));
   uint32_t hash2[8] __attribute__ ((aligned (32)));
   uint32_t hash3[8] __attribute__ ((aligned (32)));
   lyra2v3_4way_ctx_holder ctx __attribute__ ((aligned (64))); 
   memcpy( &ctx, &l2v3_4way_ctx, sizeof(l2v3_4way_ctx) );

   blake256_4way( &ctx.blake, input, 80 );
   blake256_4way_close( &ctx.blake, vhash );
   mm128_deinterleave_4x32( hash0, hash1, hash2, hash3, vhash, 256 );

   LYRA2REV3( l2v3_wholeMatrix, hash0, 32, hash0, 32, hash0, 32, 1, 4, 4 );
   LYRA2REV3( l2v3_wholeMatrix, hash1, 32, hash1, 32, hash1, 32, 1, 4, 4 );
   LYRA2REV3( l2v3_wholeMatrix, hash2, 32, hash2, 32, hash2, 32, 1, 4, 4 );
   LYRA2REV3( l2v3_wholeMatrix, hash3, 32, hash3, 32, hash3, 32, 1, 4, 4 );
   
   cubehashUpdateDigest( &ctx.cube, (byte*) hash0, (const byte*) hash0, 32 );
   cubehashReinit( &ctx.cube );
   cubehashUpdateDigest( &ctx.cube, (byte*) hash1, (const byte*) hash1, 32 );
   cubehashReinit( &ctx.cube );
   cubehashUpdateDigest( &ctx.cube, (byte*) hash2, (const byte*) hash2, 32 );
   cubehashReinit( &ctx.cube );
   cubehashUpdateDigest( &ctx.cube, (byte*) hash3, (const byte*) hash3, 32 );

   LYRA2REV3( l2v3_wholeMatrix, hash0, 32, hash0, 32, hash0, 32, 1, 4, 4 );
   LYRA2REV3( l2v3_wholeMatrix, hash1, 32, hash1, 32, hash1, 32, 1, 4, 4 );
   LYRA2REV3( l2v3_wholeMatrix, hash2, 32, hash2, 32, hash2, 32, 1, 4, 4 );
   LYRA2REV3( l2v3_wholeMatrix, hash3, 32, hash3, 32, hash3, 32, 1, 4, 4 );

   mm128_interleave_4x32( vhash, hash0, hash1, hash2, hash3, 256 );
   bmw256_4way( &ctx.bmw, vhash, 32 );
   bmw256_4way_close( &ctx.bmw, vhash );

   mm128_deinterleave_4x32( state, state+32, state+64, state+96, vhash, 256 );
}

int scanhash_lyra2rev3_4way( int thr_id, struct work *work, uint32_t max_nonce,
                             uint64_t *hashes_done )
{
   uint32_t hash[8*4] __attribute__ ((aligned (64)));
   uint32_t vdata[20*4] __attribute__ ((aligned (64)));
   uint32_t edata[20] __attribute__ ((aligned (64)));
   uint32_t *pdata = work->data;
   uint32_t *ptarget = work->target;
   const uint32_t first_nonce = pdata[19];
   uint32_t n = first_nonce;
   const uint32_t Htarg = ptarget[7];
   uint32_t *nonces = work->nonces;
   int num_found = 0;
   uint32_t *noncep = vdata + 76; // 19*4

   if ( opt_benchmark )
      ( (uint32_t*)ptarget )[7] = 0x0000ff;

   swab32_array( edata, pdata, 20 );
   mm128_interleave_4x32( vdata, edata, edata, edata, edata, 640 );

   do {
      be32enc( noncep,   n   );
      be32enc( noncep+1, n+1 );
      be32enc( noncep+2, n+2 );
      be32enc( noncep+3, n+3 );

      lyra2rev3_4way_hash( hash, vdata );
      pdata[19] = n;

      for ( int i = 0; i < 4; i++ )
      if ( (hash+(i<<3))[7] <= Htarg && fulltest( hash+(i<<3), ptarget ) )
      {
          pdata[19] = n+i;         
          nonces[ num_found++ ] = n+i;
          work_set_target_ratio( work, hash+(i<<3) );
      }
      n += 4;
   } while ( (num_found == 0) && (n < max_nonce-4)
                   && !work_restart[thr_id].restart);

   *hashes_done = n - first_nonce + 1;
   return num_found;
}

#endif
