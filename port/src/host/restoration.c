#include "restoration.h"
#include "../core/sha256.h"
#include <string.h>

#define PRG_SIZE 262144u
static const char retail_sha[]="84f5377980d2fd44d71faec42f858b1e83540c2f55aba9236c3279d6dde8592a";
typedef struct {
    size_t offset, length, resource, resource_size;
    const char *resource_sha;
    uint8_t before[7], after[7];
} patch_t;
/* High bit encodes the native trailing space. Preserve both encoded length
 * and expanded character count, including the retail credit byline spacing. */
static const patch_t ed={0x26ef7,5,0x26e70,329,
    "2ecf459b9f6ea224b578fe6b61a8f51d2a79190a90a360b6b5c7fa9f3f7256ba",
    {'s','l','e','p','t'|128}, {'e','a','t','e','n'|128}};
static const patch_t credits[]={
    {0x233f0,5,0x230bf,1663,
     "cd080fbe066b5c909851bdd4589a769b03e1d352445d56afcf26b3c1e47f591f",
     {'S','t','o','r','y'|128}, {'S','C','U','M','M'|128}},
    {0x23487,7,0x230bf,1663,
     "cd080fbe066b5c909851bdd4589a769b03e1d352445d56afcf26b3c1e47f591f",
     {'v','e','r','s','i','o','n'|128}, {'S','C','U','M','M',' ',' '|128}}
};
static bool matches(const uint8_t *prg,const patch_t *p) {
    char digest[65];sha256_hex(prg+p->resource,p->resource_size,digest);
    return !strcmp(digest,p->resource_sha)&&!memcmp(prg+p->offset,p->before,p->length);
}
restoration_result_t restoration_apply(uint8_t *prg,size_t size,bool enhanced,
                                      restoration_options_t options) {
    if(!enhanced||(!options.ed_dialogue&&!options.scumm_credits))return RESTORATION_OFF;
    if(!prg||size!=PRG_SIZE)return RESTORATION_BAD_SIZE;
    char digest[65];sha256_hex(prg,size,digest);
    if(strcmp(digest,retail_sha))return RESTORATION_WRONG_ROM;
    if(options.ed_dialogue&&!matches(prg,&ed))return RESTORATION_BAD_SIGNATURE;
    if(options.scumm_credits)for(unsigned i=0;i<2;i++)
        if(!matches(prg,&credits[i]))return RESTORATION_BAD_SIGNATURE;
    if(options.ed_dialogue)memcpy(prg+ed.offset,ed.after,ed.length);
    if(options.scumm_credits)for(unsigned i=0;i<2;i++)
        memcpy(prg+credits[i].offset,credits[i].after,credits[i].length);
    return RESTORATION_APPLIED;
}
const char *restoration_result_name(restoration_result_t r) {
    static const char *const names[]={"Off","Applied partial text references",
        "Unsupported PRG size","Retail identity mismatch","Resource signature mismatch"};
    return (unsigned)r<sizeof(names)/sizeof(*names)?names[r]:"Unsupported";
}
void restoration_identity(const uint8_t *prg,size_t size,uint8_t out[8]) {
    uint8_t digest[32];sha256(prg,size,digest);memcpy(out,digest,8);
}
