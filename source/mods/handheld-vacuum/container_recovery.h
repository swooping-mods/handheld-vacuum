/* These reads inspect existing dictionaries; they never seed native loadouts or
 * synthesize a default bag. Native Pin/Switch alone may change the ledger. */
typedef struct {PTR loaded;double active,stored,capacity;} VNBalance;
static int vn_lookup(PTR dictionary,PTR key,void *out,int *found) {
    if(!dictionary||!key)return 0;
    PTR method=get_method(object_class(dictionary),"TryGetValue",2);int ok=1;
    void *args[]={key,out};PTR result=method?call(method,dictionary,args,&ok):0;
    if(!ok||!result)return 0;
    *found=*(unsigned char *)unbox(result)!=0;return 1;
}
static int vn_number(double value){return value>=0&&value<1e9;}
static int vn_read_balance(PTR world,PTR actor,PTR bag,VNBalance *balance) {
    *balance=(VNBalance){0};int found=0;
    if(!vn_lookup(vn_field(world,"loadedContainer"),actor,&balance->loaded,&found))return 0;
    if(!vn_lookup(vn_field(world,"inventoryMass"),actor,&balance->active,&found))return 0;
    PTR nested=0;
    if(!vn_lookup(vn_field(world,"toolMass"),actor,&nested,&found))return 0;
    if(found&&(!nested||!vn_lookup(nested,bag,&balance->stored,&found)))return 0;
    int ok=1;void *args[]={actor,bag};PTR cap=call(vn.capacity,world,args,&ok);
    if(!ok||!cap)return 0;
    balance->capacity=*(double *)unbox(cap);
    return vn_number(balance->active)&&vn_number(balance->stored)&&
        balance->capacity>0&&balance->capacity<1e9&&balance->stored<=balance->capacity;
}
static int vn_backing(PTR world,PTR actor) {
    /* Guard provider-owned native getters whose fallback could create missing
     * loadout state. This does not make other game/framework callbacks read-only. */
    PTR record=0;int found=0;
    if(!vn_lookup(vn_field(world,"upgrades"),actor,&record,&found)||!found||!record)return 0;
    SURoot root=su_hold(record);if(!root)return 0;
    PTR slots=vn_field(su_get(root),"slots"),player=vn_field(su_get(root),"player");
    PTR field=get_field(object_class(su_get(root)),"selectedSlot");int slot=-1;
    if(field)field_value(su_get(root),field,&slot);
    int ready=field&&vn_equal(player,actor)&&slots&&su.array_length(slots)==6&&slot>=0&&slot<6;
    su_drop(&root);return ready;
}
static int vn_hint_context(PTR world,PTR actor) {
    int ok=1;
    if(!tt.owned||!vn.owner_actor||!vn_equal(su_get(vn.owner_actor),actor)||
       world!=su_get(tt.world)||!vc_scope(&vn.link,&vn_api,world,actor)||
       !su_alive(world)||boolean(tt.restoring,world,&ok)||!ok||!vn_authority(&ok)||
       !vn_equal(vn_player(),actor)||!vn_backing(world,actor))return 0;
    PTR tool=vn_actual_tool(world,actor,&ok),id=tool?call(vn.tool_id,tool,0,&ok):0;
    return ok&&!vn.foreign&&!vn.read_failed&&vn_vacuum(id)&&
        tt.equipment->is_equipped(TOOL_ID,su_get(tt.walker));
}
static int vn_hint_name(char *name) {
    unsigned length=0;while(length<sizeof(tt.store_name)&&tt.store_name[length])length++;
    if(!tt.store_root[0]||!length||length==sizeof(tt.store_name))return 0;
    cat(cat(name,tt.store_name),".container");return 1;
}
static void vn_remember_hint(PTR world,PTR actor) {
    if(!vc_scope(&vn.link,&vn_api,world,actor)||vn.link.quarantined||!tt.owned||vn.read_failed||
       vc_bag_valid(&vn.link,&vn_api)!=1)return;
    PTR bag=vc_get(&vn_api,vn.link.bag);VHint record;char name[192];
    if(!bag||!vh_encode(&record,su.chars(actor),(unsigned)su.string_length(actor),
       su.chars(bag),(unsigned)su.string_length(bag))||!vn_hint_name(name))return;
    if(vn.hint_seen&&vh_hash(&vn.hint)==record.check&&
       vh_decode(&vn.hint,sizeof(vn.hint),record.actor,record.actor_length)&&
       vn.hint.bag_length==record.bag_length) {
        unsigned i=0;while(i<record.bag_length&&vn.hint.bag[i]==record.bag[i])i++;
        if(i==record.bag_length)return;
    }
    /* Cache even a failed attempt: do not hammer a failing disk every frame.
     * A newly observed genuine identity permits another bounded write. */
    vn.hint=record;vn.hint_seen=1;
    if(!ss_replace_named(tt.store_root,name,(const unsigned char *)&record,sizeof(record)))
        tt_event("container-hint-save-failed");
}
static int vn_recover_hint(PTR world,PTR actor) {
    if(vn.hint_attempted||vn.link.bag||vn.link.active||vn.link.dirty||vn.link.quarantined||
       !vn.link.selected||!vn.link.owned||!vn_hint_context(world,actor))return 0;
    vn.hint_attempted=1;char name[192];VHint record;unsigned size=0;
    if(!vn_hint_name(name)||!ss_load_named(tt.store_root,name,(unsigned char *)&record,sizeof(record),&size)||
       !vh_decode(&record,size,su.chars(actor),(unsigned)su.string_length(actor)))return 0;
    SURoot root=su_hold(vn.string_utf16(record.bag,(int)record.bag_length));if(!root)return 0;
    PTR bag=su_get(root);VNBalance before,after;int success=0;
    if(vn_valid(0,world,actor,bag)!=1||!vn_read_balance(world,actor,bag,&before)||
       (before.loaded&&su.string_length(before.loaded)!=0)||before.active!=0||
       !vn_hint_context(world,actor))goto done;
    /* Pin may flush pending native crew/stroke operations. Recheck every
     * admission and raw ledger fact before allowing the alias or Switch. */
    if(!vn_pin(0,world,actor))goto uncertain;
    if(!vn_hint_context(world,actor)||vn_valid(0,world,actor,bag)!=1||
       !vn_read_balance(world,actor,bag,&after)||after.active!=0||
       after.stored!=before.stored||after.capacity!=before.capacity||
       (after.loaded&&su.string_length(after.loaded)!=0&&!vn_vacuum(after.loaded)))goto uncertain;
    vn.link.bag=root;root=0;vn.link.active=vn.link.dirty=1;
    if(!vn_switch(0,world,actor))goto uncertain;
    if(!vn_hint_context(world,actor)||vn_valid(0,world,actor,bag)!=1||
       !vn_read_balance(world,actor,bag,&after)||!vn_equal(after.loaded,bag)||
       after.active!=before.stored||after.stored!=0||after.capacity!=before.capacity)goto uncertain;
    vn.link.dirty=0;vn.link.ready=1;vn.hint=record;vn.hint_seen=1;
    tt_event("container-hint-restored");success=1;goto done;
uncertain:
    vn.link.quarantined=1;vn.link.ready=0;tt_event("container-hint-recovery-uncertain");
done:
    su_drop(&root);return success;
}
