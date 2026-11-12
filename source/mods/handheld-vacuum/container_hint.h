/* Identity only: native container storage remains the sole cargo ledger.
 * Full actor identity disambiguates the existing hashed sidecar basename. */
#ifndef MOON_VACUUM_CONTAINER_HINT_H
#define MOON_VACUUM_CONTAINER_HINT_H
#define VH_ID_MAX 127u
typedef struct {
    unsigned magic,version,actor_length,bag_length;
    unsigned short actor[VH_ID_MAX+1],bag[VH_ID_MAX+1];
    unsigned check;
} VHint;
typedef char VH_layout[(sizeof(unsigned)==4&&sizeof(unsigned short)==2&&sizeof(VHint)==532)?1:-1];
static unsigned vh_hash(const VHint *record) {
    const unsigned char *bytes=(const unsigned char *)record;unsigned hash=2166136261u;
    for(unsigned i=0;i<sizeof(*record)-sizeof(record->check);i++){hash^=bytes[i];hash*=16777619u;}return hash;
}
static int vh_id(const unsigned short *id,unsigned length) {
    if(!id||!length||length>VH_ID_MAX)return 0;
    for(unsigned i=0;i<length;i++)if(!id[i])return 0;
    return 1;
}
static int vh_encode(VHint *record,const unsigned short *actor,unsigned actor_length,
                     const unsigned short *bag,unsigned bag_length) {
    if(!record||!vh_id(actor,actor_length)||!vh_id(bag,bag_length))return 0;
    unsigned char *bytes=(unsigned char *)record;for(unsigned i=0;i<sizeof(*record);i++)bytes[i]=0;
    record->magic=0x56434849u;record->version=1;record->actor_length=actor_length;record->bag_length=bag_length;
    for(unsigned i=0;i<actor_length;i++)record->actor[i]=actor[i];
    for(unsigned i=0;i<bag_length;i++)record->bag[i]=bag[i];
    record->check=vh_hash(record);return 1;
}
static int vh_decode(const VHint *record,unsigned size,const unsigned short *actor,unsigned actor_length) {
    if(!record||size!=sizeof(*record)||record->magic!=0x56434849u||record->version!=1||
       !vh_id(actor,actor_length)||record->actor_length!=actor_length||
       !vh_id(record->actor,record->actor_length)||!vh_id(record->bag,record->bag_length)||
       record->check!=vh_hash(record))return 0;
    for(unsigned i=0;i<actor_length;i++)if(record->actor[i]!=actor[i])return 0;
    for(unsigned i=record->actor_length;i<=VH_ID_MAX;i++)if(record->actor[i])return 0;
    for(unsigned i=record->bag_length;i<=VH_ID_MAX;i++)if(record->bag[i])return 0;
    return 1;
}
#endif
