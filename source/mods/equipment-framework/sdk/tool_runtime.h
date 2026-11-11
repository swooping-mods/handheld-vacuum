/* Shared adapter for native tool providers. The frameworks own presentation;
 * each provider keeps its existing world/player ownership and feature logic. */
#ifndef MOON_TOOL_RUNTIME_H
#define MOON_TOOL_RUNTIME_H
#include "equipment.h"
#include "../../../runtime/native/input.h"
#include "client.h"
#include "../../shop-framework/sdk.h"
#include "../../shop-framework/client.h"
#include "../../hints-framework/sdk/hints.h"
#include "../../hints-framework/sdk/client.h"
#define SS_ROOT_MAX 1000u
#define SS_MAX_DIRS 512u
#define SS_PATH_MAX 1200u
#define SS_MAX_BYTES 16u
#include "../../../runtime/sdk/storage.h"
#include "../../../runtime/sdk/wallet.h"
typedef struct {float x,y;} T2;
static struct {
    PTR world_field,camera_field,component_transform,firstperson,blocked,typing,focus,frame,restoring;
    PTR joined,host,local,player,wallet,charge,decimal_int,decimal_long,save_root;
    SURoot world,camera,walker;PTR log;DWORD thread;int last_frame,owned,invalid_state,visible,gate;
    int authority_set,authority,joined_initial,suspended;U64 host_initial;
    unsigned char down[256],pressed[256];float dt;
    char store_root[1000],store_name[160];const char *status;
    void (*write_barrier)(PTR,PTR *,PTR),(*set_field)(PTR,PTR,void *);
    const MoonEquipment *equipment;const MoonShop *shop;const MoonHints *hints;
    /* Raw asset/config readers reuse the bound IO functions, outside sidecars. */
    BOOL (__stdcall *read)(PTR,void *,DWORD,DWORD *,PTR),(__stdcall *close)(PTR);
    DWORD (__stdcall *window_pid)(PTR,DWORD *);PTR (__stdcall *foreground)(void);short (__stdcall *key)(int);
} tt;
static void feature_tick(PTR walker,PTR world,int allow);
static void feature_drop(void);
#ifdef TOOL_ENV
static void config_initialize(void);
static int configured_enabled(void);
static int feature_can_buy(PTR world);
#else
static inline int feature_can_buy(PTR world){(void)world;return 1;}
#endif
void *memcpy(void *to,const void *from,U64 n){su_copy(to,from,n);return to;}
static inline void tt_event(const char *event){if(!tt.log)return;char b[256],*p=cat(b,"{\"mod\":\"" TOOL_ID "\",\"event\":\"");p=cat(p,event);p=cat(p,"\"}\n");DWORD n;WriteFile(tt.log,b,(DWORD)(p-b),&n,0);}
static inline int tt_vk(int key){if(key>=97&&key<=122)return key-32;return key==277?0x2d:key==280?0x21:key==281?0x22:key==127?0x2e:key==308||key==307?0x12:key;}
static inline MoonInputState tt_action(const char *id,int key,int allowed){char name[MOON_INPUT_NAME_CAPACITY];char *p=cat(name,TOOL_ID ".");cat(p,id);return moon_input_key_action(name,(unsigned)tt_vk(key),allowed);}
static inline int tt_down(int key){int vk=tt_vk(key);return vk>=0&&vk<256&&tt.pressed[vk];}
static inline int tt_held(int key){int vk=tt_vk(key);return vk>=0&&vk<256&&tt.down[vk];}
static inline PTR tt_transform(PTR component){int ok=1;return call(tt.component_transform,component,0,&ok);}
static inline int tt_enabled(void){
#ifdef TOOL_ENV
    return configured_enabled();
#else
    return 1;
#endif
}
static inline int tt_host(void){
    int ok=1,joined=boolean(tt.joined,0,&ok);PTR mine=call(tt.local,0,0,&ok),host=call(tt.host,0,0,&ok);
    U64 my_id=mine?*(U64 *)unbox(mine):0,host_id=host?*(U64 *)unbox(host):0;
    if(!tt.authority_set){tt.authority_set=1;tt.joined_initial=joined;tt.host_initial=host_id;tt.authority=ok&&(!joined||(my_id&&host_id==my_id));}
    if(!ok||joined!=tt.joined_initial||(joined&&host_id!=tt.host_initial))tt.suspended=1;
    return tt.authority&&!tt.suspended;
}
static inline unsigned tt_hash(const unsigned char *p,unsigned n){unsigned v=2166136261u;while(n--){v^=*p++;v*=16777619u;}return v;}
/* Save identity must never use su_string's display-oriented truncation or
 * replacement characters: either preserve every code unit or fail closed. */
static inline int tt_identity(PTR value,char *out,unsigned capacity){
    if(!value)return 0;int n=su.string_length(value);const unsigned short *chars=su.chars(value);
    if(n<1||(unsigned)n>=capacity||!chars)return 0;
    for(int i=0;i<n;i++){if(!chars[i]||chars[i]>=128)return 0;out[i]=(char)chars[i];}out[n]=0;return 1;
}
typedef struct {unsigned magic,version,owned,check;} TSave;
static inline int tt_save(void){
    TSave s={0x4d544f4f,1,(unsigned)tt.owned,0};s.check=tt_hash((unsigned char *)&s,12);
    return ss_replace_named(tt.store_root,tt.store_name,(const unsigned char *)&s,sizeof(s));
}
static inline void tt_load(PTR world){
    int ok=1;char path[1000],player[128];PTR root=call(tt.save_root,world,0,&ok),id=call(tt.player,0,0,&ok);
    if(!ok||!tt_identity(root,path,sizeof(path))||!tt_identity(id,player,sizeof(player))){tt.invalid_state=1;return;}
    unsigned length=0;while(player[length])length++;
    cat(tt.store_root,path);char *name=cat(tt.store_name,"moonlab-" TOOL_ID "-");name=num(name,tt_hash((unsigned char *)player,length));cat(name,".bin");
    TSave s={0};unsigned n=0;
    /* The existing world root and player-hashed basename remain the identity.
     * The common store pins ancestors and denies file/directory reparse points. */
    if(!ss_load_named(tt.store_root,tt.store_name,(unsigned char *)&s,sizeof(s),&n)){
        if(ss.last_error==SS_NOT_FOUND){tt.owned=0;return;}
        tt.invalid_state=1;tt.status="Ownership file is invalid; purchase disabled";return;
    }
    int valid=n==sizeof(s);
    valid=valid&&s.magic==0x4d544f4f&&s.version==1&&s.owned<=1&&s.check==tt_hash((unsigned char *)&s,12);
    if(valid)tt.owned=(int)s.owned;else{tt.invalid_state=1;tt.status="Ownership file is invalid; purchase disabled";}
}
static inline long long tt_balance(PTR world,PTR player,int *ok){void *a[]={player};PTR money=call(tt.wallet,world,a,ok);if(!*ok||!money){*ok=0;return 0;}void *b[]={unbox(money)};PTR value=call(tt.decimal_long,0,b,ok);if(!value){*ok=0;return 0;}return *(long long *)unbox(value);}
static inline int tt_charge(PTR world,PTR player,int delta){int ok=1;unsigned char decimal[16]={0};void *a[]={&delta};call(tt.decimal_int,decimal,a,&ok);void *b[]={player,decimal};if(ok)call(tt.charge,world,b,&ok);return ok;}
typedef struct {PTR world,player;} TTWalletContext;
static inline int tt_wallet_balance(void *context,long long *amount){TTWalletContext *c=context;int ok=1;*amount=tt_balance(c->world,c->player,&ok);return ok;}
static inline int tt_wallet_adjust(void *context,int delta){TTWalletContext *c=context;return tt_charge(c->world,c->player,delta);}
static inline int tt_commit_purchase(void *context){(void)context;tt.owned=1;if(tt_save())return 1;tt.owned=0;return 0;}
static inline int tt_buy(PTR world){
    if(tt.owned||tt.invalid_state||!tt_enabled()||!feature_can_buy(world)||!tt_host())return 0;
    int ok=1;PTR player=call(tt.player,0,0,&ok);if(!ok||!player)return 0;
    SURoot rooted=su_hold(player);if(!rooted)return 0;
    TTWalletContext context={world,su_get(rooted)};const MoonWallet wallet={tt_wallet_balance,tt_wallet_adjust};
    int result=mw_purchase(&wallet,&context,TOOL_PRICE,tt_commit_purchase);su_drop(&rooted);
    if(result==MW_PAID){tt.status="Purchased; assign in Equipment inventory";tt_event("purchased");return 1;}
    if(result==MW_DEBIT_UNCONFIRMED){tt.status="Balance confirmation failed";tt.invalid_state=1;tt_event("debit-unconfirmed");return -1;}
    if(result==MW_SAVE_REFUNDED||result==MW_REFUND_UNCONFIRMED){tt.status=result==MW_SAVE_REFUNDED?"Could not save ownership; refunded":"Save failed; refund unconfirmed";if(result==MW_REFUND_UNCONFIRMED)tt.invalid_state=1;tt_event("ownership-save-failed");return -1;}
    tt.status=result==MW_DEBIT_REFUSED?"Purchase refused":"Insufficient funds";return 0;
}
static inline PTR tt_component(PTR go,const char *ns,const char *name){int ok=1;void *a[]={su_type(ns,name)};return a[0]?call(su.go_add,go,a,&ok):0;}
static inline void tt_shop_text(const char *active){
    if(tt.visible&&tt.hints)tt.hints->publish(TOOL_ID,su_get(tt.walker),active);
    else if(tt.hints)tt.hints->clear(TOOL_ID);
}
static inline PTR tt_material(SUColor color){int ok=1;void *a[]={su.string("Universal Render Pipeline/Unlit")};PTR shader=call(su.shader_find,0,a,&ok);if(!shader){a[0]=su.string("Sprites/Default");shader=call(su.shader_find,0,a,&ok);}PTR m=shader?su_new("UnityEngine","Material"):0;void *b[]={shader};call(su.material_ctor,m,b,&ok);void *c[]={&color};call(su.material_color,m,c,&ok);return ok?m:0;}
static inline void tt_assign(PTR go,PTR material){PTR array=su.new_array(su.material_class,1);if(!array)return;PTR *items=su.array_address(array,8,0);tt.write_barrier(array,items,material);int ok=1;void *a[]={array};call(su.renderer_materials,su_component(go,"Renderer"),a,&ok);}
static inline PTR tt_primitive(PTR parent,int shape,SPVec3 p,SPVec3 scale,PTR material){int ok=1;void *a[]={&shape};PTR go=call(su.primitive,0,a,&ok);if(!go)return 0;su_bool(su.collider_enabled,su_component(go,"Collider"),0);su_int(su_method("UnityEngine","GameObject","set_layer","System.Int32"),go,2);PTR tr=su_transform(go);if(parent)su_parent(tr,parent);su_set_vector(su.tr_local_position,tr,p);su_set_vector(su.tr_scale,tr,scale);if(material)tt_assign(go,material);return go;}
static inline int tt_ray(SPVec3 origin,SPVec3 direction,float range,unsigned char *hit){int ok=1,mask=~(1<<2),trigger=1;su_zero(hit,64);void *a[]={&origin,&direction,hit,&range,&mask,&trigger};PTR v=call(su.physics_ray,0,a,&ok);return ok&&v&&*(unsigned char *)unbox(v);}
static void tt_teardown(void){feature_drop();if(tt.hints)tt.hints->clear(TOOL_ID);su_drop(&tt.world);su_drop(&tt.camera);su_drop(&tt.walker);tt.owned=tt.invalid_state=tt.visible=tt.authority_set=tt.authority=tt.suspended=0;tt.store_root[0]=tt.store_name[0]=0;tt.status=0;tt_event("world-teardown");}
static int tt_context(PTR world,PTR walker){
    if(tt.thread&&tt.thread!=GetCurrentThreadId())return 0;tt.thread=GetCurrentThreadId();PTR camera=0;field_value(walker,tt.camera_field,&camera);
    if(tt.world&&(world!=su_get(tt.world)||walker!=su_get(tt.walker)||camera!=su_get(tt.camera)||!su_alive(world)))tt_teardown();
    if(!world||!su_alive(world)||!su_alive(camera))return 0;int ok=1;if(boolean(tt.restoring,world,&ok)||!ok)return 0;
    if(!tt.world){tt.world=su_hold(world);tt.camera=su_hold(camera);tt.walker=su_hold(walker);tt_load(world);(void)tt_host();tt_event("world-ready");}return 1;
}
static int tt_configure(void *definition,void *world){
    (void)world;PTR field=get_field(object_class(definition),"visualScale");if(!field)return 0;
    /* These providers draw their own held model (or no held model). Keep the
     * borrowed brush prefab from appearing alongside the provider's artwork. */
    float hidden=0;tt.set_field(definition,field,&hidden);return 1;
}
static int tt_is_owned(void *world,void *walker){return tt_context(world,walker)&&tt_enabled()&&tt.owned&&!tt.invalid_state;}
static unsigned tt_offer_state(void *world,void *walker){
    if(!tt_context(world,walker)||!tt_enabled())return MS_BLOCKED;
    if(tt.owned)return MS_AVAILABLE|MS_OWNED;
    return MS_AVAILABLE|((tt.invalid_state||!tt_host()||!feature_can_buy(world))?MS_BLOCKED:0);
}
static int tt_purchase(void *world,void *walker){return tt_context(world,walker)?tt_buy(world):0;}
static int tt_update(int id,Context *context){
    (void)id;int ok=1,frame=integer(tt.frame,0,&ok);if(!ok||frame==tt.last_frame)return 0;tt.last_frame=frame;
    for(int k=0;k<256;k++){int down=moon_input_key((unsigned)k);tt.pressed[k]=(unsigned char)(down&&!tt.down[k]);tt.down[k]=(unsigned char)down;}
    PTR walker=(PTR)context->rcx,world=0;field_value(walker,tt.world_field,&world);
    if(!tt_context(world,walker)){if(tt.world)feature_tick(walker,world,0);if(tt.hints)tt.hints->clear(TOOL_ID);return 0;}
    tt.dt=su_number(su.time_delta,0);if(tt.dt<0)tt.dt=0;if(tt.dt>.1f)tt.dt=.1f;
    DWORD pid=0;tt.window_pid(tt.foreground(),&pid);int allow=tt_enabled()&&pid==GetCurrentProcessId()&&boolean(tt.focus,0,&ok)&&boolean(tt.firstperson,walker,&ok)&&!boolean(tt.blocked,walker,&ok)&&!boolean(tt.typing,0,&ok)&&ok;
    tt.visible=allow&&tt.owned&&tt.equipment->is_equipped(TOOL_ID,walker);
    if(!tt.visible)tt.hints->clear(TOOL_ID);
    feature_tick(walker,world,tt.visible);return 0;
}
static inline int tt_start(PTR domain,PTR game){
    if(!bind_runtime(domain,game)||!su_bind(game)||!ss_bind())return 0;
    tt.write_barrier=(void(*)(PTR,PTR *,PTR))GetProcAddress(game,"il2cpp_gc_wbarrier_set_field");tt.set_field=(void(*)(PTR,PTR,void *))GetProcAddress(game,"il2cpp_field_set_value");if(!tt.write_barrier||!tt.set_field)return 0;
    tt.read=ss.read;tt.close=ss.close;
    PTR user=LoadLibraryA("user32.dll");
    tt.key=(short(__stdcall *)(int))GetProcAddress(user,"GetAsyncKeyState");tt.foreground=(PTR(__stdcall *)(void))GetProcAddress(user,"GetForegroundWindow");tt.window_pid=(DWORD(__stdcall *)(PTR,DWORD *))GetProcAddress(user,"GetWindowThreadProcessId");if(!tt.key||!tt.foreground||!tt.window_pid)return 0;
    char folder[1000],path[1100];DWORD n=GetEnvironmentVariableA("MOONLAB_EVIDENCE",folder,sizeof(folder));if(n&&n<sizeof(folder)){cat(cat(path,folder),"\\" TOOL_ID ".jsonl");tt.log=CreateFileA(path,0x40000000,7,0,2,0x80,0);if(tt.log==(PTR)(U64)-1)tt.log=0;}
#define TB(F,NS,T,N,S) tt.F=su_method(NS,T,N,S);if(!tt.F){tt_event(T "::" N " binding-failed");return 0;}
    TB(component_transform,"UnityEngine","Component","get_transform","");
    TB(firstperson,"MoonCleaners","MoonWalker","get_IsFirstPerson","");TB(blocked,"MoonCleaners","MoonWalker","get_MenuBlocked","");TB(typing,"MoonCleaners","MoonInput","get_Typing","");TB(restoring,"MoonCleaners","MoonWorld","get_IsRestoringRendering","");
    TB(focus,"UnityEngine","Application","get_isFocused","");TB(frame,"UnityEngine","Time","get_frameCount","");TB(joined,"MoonCleaners","MoonCrew","get_Joined","");TB(host,"MoonCleaners","MoonCrew","get_HostId","");TB(local,"Steamworks","SteamUser","GetSteamID","");TB(player,"MoonCleaners","MoonCrew","get_LocalPlayerId","");
    TB(wallet,"MoonCleaners","MoonWorld","WalletDollars","System.String");TB(charge,"MoonCleaners","MoonWorld","ChargeBuild","System.String;System.Decimal");TB(save_root,"MoonCleaners","MoonWorld","get_SaveDirectory","");TB(decimal_int,"System","Decimal",".ctor","System.Int32");TB(decimal_long,"System","Decimal","ToInt64","System.Decimal");
#undef TB
    PTR walker=find_class("MoonCleaners","MoonWalker");tt.world_field=get_field(walker,"world");tt.camera_field=get_field(walker,"viewCamera");if(!tt.world_field||!tt.camera_field)return 0;
#ifdef TOOL_ENV
    config_initialize();
#endif
    tt.equipment=me_connect(domain,game);tt.shop=ms_connect(domain,game);tt.hints=mh_connect(domain,game);tt.last_frame=-1;
    return tt.equipment&&tt.shop&&tt.hints;
}
static inline int tt_install(PTR game){
    MoonEquipmentDefinition equipment={ME_ABI,sizeof(equipment),TOOL_ID,TOOL_NAME,TOOL_DESCRIPTION,0,tt_is_owned,tt_configure};
    MoonShopDefinition offer={MS_ABI,sizeof(offer),TOOL_ID,TOOL_NAME,TOOL_DESCRIPTION,TOOL_PRICE,tt_offer_state,tt_purchase};
    if(!tt.equipment->register_item(&equipment)||!tt.shop->register_item(&offer))return 0;
    if(!prepare_hook(method("MoonCleaners","MoonWalker","Update",0),100671733,0,game))return 0;before_hook=tt_update;if(!install_hooks())return 0;tt_event("ready");return 1;
}
#endif
