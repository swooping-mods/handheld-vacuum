#include "../../runtime/native/local_actor.h"
#include "container_link.h"
#include "container_hint.h"
static struct {
    PTR held_id,held_tool,loaded_id,tool_level,capacity,pin,switch_container,selected,tool_id;
    PTR (*string_utf16)(const unsigned short *,int);
    VCLink link;SURoot owner_actor,authority_world;DWORD owner_thread;int busy,foreign,read_failed;
    int authority_latched,authority_denied,authority_joined;U64 authority_host;
    SURoot candidate_world;int candidate_latched,candidate_denied,candidate_joined;U64 candidate_host;
    VHint hint;int hint_attempted,hint_seen;
} vn;
static int vn_equal(PTR a,PTR b) {
    if(a==b)return 1;if(!a||!b)return 0;
    int n=su.string_length(a);if(n<0||n>127||su.string_length(b)!=n)return 0;
    const unsigned short *x=su.chars(a),*y=su.chars(b);
    for(int i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;
}
static int vn_vacuum(PTR id) {
    static const char name[]=TOOL_ID;int n=id?su.string_length(id):0;
    if(n!=(int)sizeof(name)-1)return 0;const unsigned short *chars=su.chars(id);
    for(int i=0;i<n;i++)if(chars[i]!=(unsigned char)name[i])return 0;return 1;
}
static PTR vn_field(PTR object,const char *name) {
    PTR value=0,field=object?get_field(object_class(object),name):0;
    if(field)field_value(object,field,&value);return value;
}
/* Unity's boolean convenience helper merges invocation failure with death.
 * Only a confirmed false result may release a retained world context. */
static int vn_liveness(PTR world) {
    if(!world)return -1;
    int ok=1;void *args[]={world};PTR value=call(su.obj_alive,0,args,&ok);
    PTR raw=ok&&value?unbox(value):0;
    return raw?(*(unsigned char *)raw?1:0):-1;
}
static PTR vn_raw(PTR method,PTR world,void **args,int *ok) {
    vn.busy++;PTR value=call(method,world,args,ok);vn.busy--;return value;
}
static PTR vn_actual_tool(PTR world,PTR player,int *ok) {
    vn.foreign=0;
    PTR walker=su_get(tt.walker);if(!walker||!su_alive(walker)||!moon_local_actor_admit(walker)||world!=su_get(tt.world)){*ok=0;return 0;}
    void *args[]={su_type("MoonCleaners","MoonToolbelt")};
    PTR belt=call(vacuum.inventory_component,walker,args,ok);
    if(!*ok||!belt){*ok=0;return 0;}
    if(su_alive(vn_field(belt,"prototypeTool"))){vn.foreign=1;return 0;}
    PTR selected=call(vn.selected,belt,0,ok);if(!*ok)return 0;void *owner[]={player};
    PTR actual=call(vn.held_tool,world,owner,ok);
    if(*ok&&actual!=selected){vn.foreign=1;return 0;}
    return *ok?actual:0;
}
static VCRef vn_hold(void *user,void *object){(void)user;return su_hold(object);}
static void *vn_get(void *user,VCRef root){(void)user;return su_get(root);}
static void vn_drop(void *user,VCRef root){(void)user;SURoot held=root;su_drop(&held);}
static int vn_same(void *user,void *a,void *b){(void)user;return vn_equal(a,b);}
static int vn_alive(void *user,void *world){(void)user;return vn_liveness(world);}
static int vn_valid(void *user,void *world,void *player,void *id) {
    (void)user;if(!id||vn_vacuum(id))return 0;
    /* The original settings catalog excludes synthetic equipment clones.
     * Positive catalog capacity does not establish player ownership. */
    PTR settings=vn_field(world,"settings"),catalog=vn_field(settings,"upgradeCatalog");
    PTR tools=vn_field(catalog,"tools");if(!tools||su.array_length(tools)>512){vn.read_failed=1;return -1;}
    int native=0,ok=1;
    for(U64 i=0;i<su.array_length(tools);i++){
        PTR *entry=su.array_address(tools,8,i),definition=entry?vn_field(*entry,"tool"):0;
        PTR key=definition?call(vn.tool_id,definition,0,&ok):0;
        if(!ok){vn.read_failed=1;return -1;}
        if(vn_equal(key,id)){
            PTR field=get_field(object_class(definition),"containerKg");float kg=0;
            if(!field){vn.read_failed=1;return -1;}
            field_value(definition,field,&kg);native=kg>0&&kg<1e9f;break;
        }
    }
    if(!native)return 0;
    void *ownership[]={id,player};PTR level=call(vn.tool_level,world,ownership,&ok);
    if(!ok||!level){vn.read_failed=1;return -1;}
    if(*(int *)unbox(level)<0)return 0;
    void *args[]={player,id};PTR result=call(vn.capacity,world,args,&ok);
    if(!ok||!result){vn.read_failed=1;return -1;}
    double capacity=ok&&result?*(double *)unbox(result):0;
    return ok&&capacity>0&&capacity<1e9;
}
static void *vn_held(void *user,void *world,void *player,int *ok) {
    (void)user;if(!vn_actual_tool(world,player,ok)){if(!*ok)vn.read_failed=1;return 0;}
    void *args[]={player};PTR id=vn_raw(vn.held_id,world,args,ok);
    if(!*ok)vn.read_failed=1;
    return *ok?id:0;
}
static void *vn_loaded(void *user,void *world,void *player,int *ok) {
    (void)user;void *args[]={player};PTR id=vn_raw(vn.loaded_id,world,args,ok);
    if(!*ok)vn.read_failed=1;
    return *ok?id:0;
}
static int vn_pin(void *user,void *world,void *player) {
    (void)user;int ok=1;void *args[]={player};call(vn.pin,world,args,&ok);return ok&&!vn.read_failed;
}
static int vn_switch(void *user,void *world,void *player) {
    (void)user;int ok=1;void *args[]={player};call(vn.switch_container,world,args,&ok);return ok&&!vn.read_failed;
}
static const VCAPI vn_api={0,vn_hold,vn_get,vn_drop,vn_same,vn_alive,vn_valid,vn_held,vn_loaded,vn_pin,vn_switch};
static PTR vn_player(void) {
    int ok=1;PTR player=call(tt.player,0,0,&ok);
    if(!ok||!player){vn.read_failed=1;return 0;}
    int length=su.string_length(player);
    if(length<=0||length>127){vn.read_failed=1;return 0;}return player;
}
static int vn_authority(int *ok);
#include "container_recovery.h"
static int vn_authority_read(PTR world,int *ok,int *latched,int *denied,int *initial_joined,U64 *initial_host) {
    /* A candidate uses its own sticky baseline until admission commits. Failed
     * replacement reads must not erase either world's authority history. */
    if(vn_liveness(world)!=1){*denied=1;return 0;}
    int joined=boolean(tt.joined,0,ok);if(!*ok){*denied=1;return 0;}
    PTR mine=call(tt.local,0,0,ok);if(!*ok||!mine){*ok=0;*denied=1;return 0;}
    PTR host=call(tt.host,0,0,ok);if(!*ok||!host){*ok=0;*denied=1;return 0;}
    U64 my_id=*(U64 *)unbox(mine),host_id=*(U64 *)unbox(host);
    if(!*latched){*latched=1;*initial_joined=joined;*initial_host=host_id;}
    if(!tt.authority_set||!tt.authority||tt.suspended||joined!=tt.joined_initial||
       joined!=*initial_joined||(joined&&(!my_id||host_id!=my_id||
       host_id!=tt.host_initial||host_id!=*initial_host)))*denied=1;
    return !*denied;
}
static int vn_authority(int *ok) {
    PTR world=su_get(tt.world);
    if(!world)return 0;
    if(!vn.authority_world)vn.authority_world=su_hold(world);
    if(!vn.authority_world||su_get(vn.authority_world)!=world)return 0;
    return vn_authority_read(world,ok,&vn.authority_latched,&vn.authority_denied,
        &vn.authority_joined,&vn.authority_host);
}
static void vn_candidate_drop(void) {
    su_drop(&vn.candidate_world);vn.candidate_latched=vn.candidate_denied=0;
    vn.candidate_joined=0;vn.candidate_host=0;
}
static int vn_candidate_authority(PTR world,int *ok) {
    if(vn.candidate_world&&su_get(vn.candidate_world)!=world){
        /* One bounded pending context: a live/unknown candidate cannot be
         * evicted by another raw callback to forget its refusal or baseline. */
        if(vn_liveness(su_get(vn.candidate_world))!=0)return 0;
        vn_candidate_drop();
    }
    if(!vn.candidate_world)vn.candidate_world=su_hold(world);
    if(!vn.candidate_world)return 0;
    return vn_authority_read(world,ok,&vn.candidate_latched,&vn.candidate_denied,
        &vn.candidate_joined,&vn.candidate_host);
}
static void vn_frame(PTR walker,PTR world) {
    vn.read_failed=0;vn.link.ready=0;PTR player=vn_player();int ok=1;
    if(!player||world!=su_get(tt.world)||vn_liveness(world)!=1||boolean(tt.restoring,world,&ok)||!ok)return;
    if(!vn_backing(world,player)){tt.status="Container loadout unavailable";return;}
    PTR previous=vn.authority_world?su_get(vn.authority_world):vc_get(&vn_api,vn.link.world);
    int replacement=previous!=world;
    if(replacement){if(!vn_candidate_authority(world,&ok))return;}
    else if(!vn_authority(&ok))return;
    if(!replacement&&vn.owner_actor&&!vn_equal(su_get(vn.owner_actor),player)){
        tt.status="Player context changed; reload the world before using the Vacuum";return;
    }
    PTR actual=vn_actual_tool(world,player,&ok),id=actual?call(vn.tool_id,actual,0,&ok):0;
    if(!ok){tt.status="Container context unavailable";return;}
    /* Allocate every persistent reference before releasing the old admitted
     * context. vc_update then sees an established scope and cannot reset it
     * while trying to allocate replacement world/player roots. */
    if(!replacement&&vn.link.quarantined){tt.status="Container operation interrupted; leave this world before using the Vacuum";return;}
    SURoot owner=0;VCLink staged={0};
    if(replacement||!vn.owner_actor){owner=su_hold(player);if(!owner)return;}
    if(replacement||!vc_scope(&vn.link,&vn_api,world,player)){
        staged.world=su_hold(world);staged.actor=su_hold(player);
        if(!staged.world||!staged.actor){vc_forget(&staged,&vn_api);su_drop(&owner);return;}
    }
    if(replacement){
        vc_forget(&vn.link,&vn_api);su_drop(&vn.owner_actor);vn.hint_attempted=vn.hint_seen=0;
        su_drop(&vn.authority_world);vn.authority_world=vn.candidate_world;vn.candidate_world=0;
        vn.authority_latched=vn.candidate_latched;vn.authority_denied=vn.candidate_denied;
        vn.authority_joined=vn.candidate_joined;vn.authority_host=vn.candidate_host;
        vn_candidate_drop();
    }
    if(owner)vn.owner_actor=owner;
    if(staged.world){vc_forget(&vn.link,&vn_api);vn.link=staged;}
    if(vn.link.quarantined){tt.status="Container operation interrupted; leave this world before using the Vacuum";return;}
    int foreign=vn.foreign;
    if(foreign){vn.link.active=vn.link.selected=vn.link.ready=vn.link.dirty=0;return;}
    int selected=vn_vacuum(id)&&tt.equipment->is_equipped(TOOL_ID,walker);
    int ready=vc_update(&vn.link,&vn_api,world,player,selected,tt.owned);
    if(!ready&&selected)ready=vn_recover_hint(world,player);
    vn_remember_hint(world,player);
    if(!ready&&selected)
        tt.status="Select an owned dust container before selecting the Vacuum";
}
static int vn_can_collect(void){return vn.link.ready&&!vn.link.quarantined&&!vn.read_failed&&!vn.authority_denied;}
static void vn_release(void) {
    vn.link.ready=0;
    if(!vn.link.world)return;
    vn.read_failed=0;PTR world=vc_get(&vn_api,vn.link.world),player=vn_player(),current=0;
    PTR walker=su_get(tt.walker);if(walker)field_value(walker,tt.world_field,&current);
    int life=vn_liveness(world);
    if(life==0){vc_forget(&vn.link,&vn_api);return;}
    /* A raw replacement walker is not an admitted replacement context. */
    if(life!=1||current!=world)return;
    if(vn.link.quarantined)return;
    int ok=1;
    if(vc_scope(&vn.link,&vn_api,current,player)&&!boolean(tt.restoring,world,&ok)&&ok&&vn_authority(&ok)) {
        if(!vn_backing(world,player)){vn.link.ready=0;return;}
        PTR actual=vn_actual_tool(world,player,&ok);
        if(ok&&actual)(void)call(vn.tool_id,actual,0,&ok);
        if(!ok){vn.link.ready=0;return;}
        if(vn.foreign){vn.link.active=vn.link.selected=vn.link.dirty=0;return;}
        if(!vc_detach(&vn.link,&vn_api)){vn.link.selected=vn.link.owned=vn.link.ready=0;tt_event("container-ledger-restore-failed");return;}
    }
}
static void vn_teardown(void) {
    PTR admitted=vn.authority_world?su_get(vn.authority_world):vc_get(&vn_api,vn.link.world);
    int dead=admitted&&vn_liveness(admitted)==0;
    vn_release();
    if(dead){
        vc_forget(&vn.link,&vn_api);su_drop(&vn.owner_actor);vn.hint_attempted=vn.hint_seen=0;
        su_drop(&vn.authority_world);vn.authority_latched=vn.authority_denied=0;
    }
    if(vn.candidate_world&&vn_liveness(su_get(vn.candidate_world))==0)vn_candidate_drop();
}
static int vn_before(int id,Context *context) {
    unsigned int current=GetCurrentThreadId();
    unsigned int owner=__atomic_load_n(&vn.owner_thread,__ATOMIC_ACQUIRE);
    if(id==0) {
        if((owner&&owner!=current)||!moon_local_actor_admit((PTR)context->rcx))return 0;
        if(!owner) {
            unsigned int expected=0;
            if(__atomic_compare_exchange_n(&vn.owner_thread,&expected,current,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED))owner=current;
            else owner=expected;
        }
        if(owner!=current)return 0;
        /* Our shared adapter asks equipment visibility before feature_tick.
         * Admit existing raw backing before allowing that provider-owned probe. */
        PTR world=0;field_value((PTR)context->rcx,tt.world_field,&world);
        PTR player=vn_player();
        if(!world||!su_alive(world)||!player||!vn_backing(world,player)){vn.link.ready=0;return 0;}
        return tt_update(id,context);
    }
    if(id!=1||!owner||current!=owner)return 0;
    if(vn.busy||!tt.thread||GetCurrentThreadId()!=tt.thread||!tt.owned)return 0;
    PTR world=(PTR)context->rcx,player=(PTR)context->rdx;
    if(!vn.owner_actor||!vn_equal(su_get(vn.owner_actor),player)||!vc_scope(&vn.link,&vn_api,world,player)||world!=su_get(tt.world))return 0;
    vn.busy++;
    int ok=1;
    if(!su_alive(world)||boolean(tt.restoring,world,&ok)||!ok||!vn_authority(&ok)){vn.read_failed=1;vn.busy--;return 0;}
    PTR local=vn_player();
    if(!vn_equal(local,player)){vn.read_failed=1;vn.busy--;return 0;}
    if(!vn_backing(world,player)){vn.read_failed=1;vn.busy--;return 0;}
    PTR tool=vn_actual_tool(world,player,&ok);
    PTR key=tool?call(vn.tool_id,tool,0,&ok):0;
    PTR alias=ok?vc_resolve(&vn.link,&vn_api,world,player,vn_vacuum(key)):0;
    if(!ok||vn.foreign)vn.read_failed=1;
    vn.busy--;
    if(!alias)return 0;
    context->rax=(U64)alias;return 1;
}
static int vn_bind(PTR game) {
    vn.string_utf16=(PTR(*)(const unsigned short *,int))GetProcAddress(game,"il2cpp_string_new_utf16");
    vn.held_id=su_method("MoonCleaners","MoonWorld","HeldContainerId","System.String");
    vn.held_tool=su_method("MoonCleaners","MoonWorld","HeldTool","System.String");
    vn.loaded_id=su_method("MoonCleaners","MoonWorld","LoadedContainerId","System.String");
    vn.tool_level=su_method("MoonCleaners","MoonWorld","ToolLevel","System.String;System.String");
    vn.capacity=su_method("MoonCleaners","MoonWorld","ContainerCapacityKg","System.String;System.String");
    vn.pin=su_method("MoonCleaners","MoonWorld","PinContainer","System.String");
    vn.switch_container=su_method("MoonCleaners","MoonWorld","SwitchContainer","System.String");
    vn.selected=su_method("MoonCleaners","MoonToolbelt","get_SelectedTool","");
    vn.tool_id=su_method("MoonCleaners","MoonToolDefinition","get_Id","");
    return vn.string_utf16&&moon_local_actor_bind()&&vn.held_id&&method_token(vn.held_id)==100665672&&vn.held_tool&&vn.loaded_id&&vn.tool_level&&vn.capacity&&vn.pin&&vn.switch_container&&vn.selected&&vn.tool_id;
}
static int vn_install(PTR game) {
    /* ModStart runs on the loader worker. The first shared Unity update
     * atomically establishes the owner before mutable binding state is read. */
    /* Existing public registration interfaces allow this provider's extra
     * native callback without changing the shared gadget adapter/defaults. */
    MoonEquipmentDefinition definition={ME_ABI,sizeof(definition),TOOL_ID,TOOL_NAME,TOOL_DESCRIPTION,0,tt_is_owned,tt_configure};
    MoonShopDefinition offer={MS_ABI,sizeof(offer),TOOL_ID,TOOL_NAME,TOOL_DESCRIPTION,TOOL_PRICE,tt_offer_state,tt_purchase};
    if(!tt.equipment->register_item(&definition)||!tt.shop->register_item(&offer))return 0;
    if(!prepare_hook(method("MoonCleaners","MoonWalker","Update",0),100671733,0,game)||!prepare_hook(vn.held_id,100665672,1,game))return 0;
    before_hook=vn_before;if(!install_hooks())return 0;tt_event("ready");return 1;
}
