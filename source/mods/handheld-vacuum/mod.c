#include "../../runtime/native/gameplay.h"
#define TOOL_ID "handheld-vacuum"
#define TOOL_DESCRIPTION "Handheld dust capsule vacuum. Select in Equipment and hold R to collect nearby capsules."
#define TOOL_NAME "Handheld Vacuum"
#define TOOL_PRICE 150
#define TOOL_KEY 277
#define TOOL_HOTKEY "Insert"
#define TOOL_ROW 2
#include "tool_runtime.h"
int _fltused=0;
static struct {PTR find,view_world,view_id,collect,inventory_component,inventory_full,inventory_world;SURoot gun,material;int equipped;float timer;unsigned collected;double kg;} vacuum;
static void vacuum_visual(int show){
    if(!show){su_destroy(su_get(vacuum.gun));su_drop(&vacuum.gun);return;}if(vacuum.gun)return;
    if(!vacuum.material)vacuum.material=su_hold(tt_material((SUColor){.22f,.32f,.34f,1}));PTR go=su_gameobject("Handheld capsule vacuum",tt_transform(su_get(tt.camera)));vacuum.gun=su_hold(go);PTR tr=su_transform(go);su_set_vector(su.tr_local_position,tr,su_v(.23f,-.22f,.48f));
    tt_primitive(tr,3,su_v(0,0,0),su_v(.18f,.15f,.25f),su_get(vacuum.material));tt_primitive(tr,3,su_v(0,.015f,.20f),su_v(.09f,.065f,.24f),su_get(vacuum.material));tt_primitive(tr,3,su_v(0,-.10f,-.04f),su_v(.065f,.15f,.07f),su_get(vacuum.material));
}
static void vacuum_collect(PTR walker,PTR world){
    if(!tt_host()){tt.status="Host-only collection in this prototype";return;}int ok=1;PTR type=su_type("MoonCleaners","MoonDustInventory");void *c[]={type};PTR inventory=call(vacuum.inventory_component,walker,c,&ok);
    if(!ok||!inventory||boolean(vacuum.inventory_full,inventory,&ok)||!ok){tt.status="Dust container full or unavailable";return;}PTR iw=call(vacuum.inventory_world,inventory,0,&ok);if(!ok||iw!=world)return;
    int inactive=0,sort=0;void *args[]={su_type("MoonCleaners","MoonDustCanisterView"),&inactive,&sort};SURoot found=su_hold(call(vacuum.find,0,args,&ok));if(!ok||!found)return;
    PTR tr=tt_transform(su_get(tt.camera));SPVec3 origin=su_vector(su.tr_get_position,tr),forward=su_vector(su.tr_forward,tr),walker_pos=su_vector(su.tr_get_position,tt_transform(walker));
    U64 count=su.array_length(su_get(found));if(count>512)count=512;float best=6.01f;int best_id=-1;SPVec3 best_position={0};
    for(U64 i=0;i<count;i++){
        PTR *entry=su.array_address(su_get(found),8,i),view=entry?*entry:0;if(!su_alive(view))continue;PTR vw=call(vacuum.view_world,view,0,&ok);if(!ok||vw!=world)continue;
        SPVec3 position=su_vector(su.tr_get_position,tt_transform(view)),delta=su_sub(position,origin);float distance=su_sqrt(su_dot(delta,delta));if(distance<.05f||distance>=best||distance>6.f)continue;
        SPVec3 direction=su_mul(delta,1/distance);if(su_dot(direction,forward)<.9659f)continue;
        unsigned char hit[64];if(tt_ray(origin,direction,distance-.16f,hit))continue;
        int id=integer(vacuum.view_id,view,&ok);if(!ok||id<0)continue;best=distance;best_id=id;best_position=position;
    }
    su_drop(&found);if(best_id<0){tt.status="Hold R: aim at a dust capsule within 6 m";return;}
    PTR player=call(tt.player,0,0,&ok);if(!ok||!player)return;
    /* The vacuum's bounded, unobstructed intake is the collection origin. Use
     * vanilla collection so cargo, capacity, removal and world replication stay
     * under the game's ownership; never destroy a capsule presentation alone. */
    void *collect[]={player,&best_id,&best_position};PTR result=call(vacuum.collect,world,collect,&ok);double kg=ok&&result?*(double *)unbox(result):0;
    (void)walker_pos;if(kg>0&&kg<1.0e12){vacuum.collected++;vacuum.kg+=kg;tt.status="Capsule collected into equipped dust container";tt_event("capsule-collected");}else tt.status="Capsule refused by normal inventory rules";
}
static void feature_drop(void){vacuum_visual(0);su_destroy(su_get(vacuum.material));su_drop(&vacuum.material);vacuum.equipped=0;vacuum.timer=0;vacuum.collected=0;vacuum.kg=0;}
static void feature_tick(PTR walker,PTR world,int allow){
    vacuum.equipped=allow&&tt.owned;int active=allow&&tt.owned&&vacuum.equipped;vacuum_visual(active);
    MoonInputState collect=tt_action("collect",114,active);vacuum.timer+=tt.dt;if(collect.held&&vacuum.timer>=.2f){vacuum.timer=0;vacuum_collect(walker,world);}
    char b[300],*p=cat(b,vacuum.equipped?"R: Vacuum capsules  |  6 m intake\n":"Select vacuum in Equipment\n");p=num(p,vacuum.collected);p=cat(p," capsules collected this session");if(tt.status){p=cat(p,"\n");cat(p,tt.status);}tt_shop_text(b);
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(!tt_start(domain,game))return 0;vacuum.find=su_method("UnityEngine","Object","FindObjectsByType","System.Type;UnityEngine.FindObjectsInactive;UnityEngine.FindObjectsSortMode");vacuum.view_world=su_method("MoonCleaners","MoonDustCanisterView","get_World","");vacuum.view_id=su_method("MoonCleaners","MoonDustCanisterView","get_Id","");vacuum.collect=su_method("MoonCleaners","MoonWorld","CollectDustCanister","System.String;System.Int32;UnityEngine.Vector3");vacuum.inventory_component=su_method("UnityEngine","Component","GetComponent","System.Type");vacuum.inventory_full=su_method("MoonCleaners","MoonDustInventory","get_IsFull","");vacuum.inventory_world=su_method("MoonCleaners","MoonDustInventory","get_World","");
    return vacuum.find&&vacuum.view_world&&vacuum.view_id&&vacuum.collect&&method_token(vacuum.collect)==100664765&&vacuum.inventory_component&&vacuum.inventory_full&&vacuum.inventory_world&&tt_install(game);
}
