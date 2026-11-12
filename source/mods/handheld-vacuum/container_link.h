/* Own only a local actor's container identity. Native Pin/Switch retain their
 * normal mass ledger; no capacity or inventory field is written here. */
#ifndef MOON_VACUUM_CONTAINER_LINK_H
#define MOON_VACUUM_CONTAINER_LINK_H
typedef unsigned long long VCRef;
typedef struct {
    void *user;
    VCRef (*hold)(void *,void *);
    void *(*get)(void *,VCRef);
    void (*drop)(void *,VCRef);
    int (*same)(void *,void *,void *);
    int (*alive)(void *,void *); /* 1 live, 0 confirmed dead, -1 unknown */
    int (*valid_bag)(void *,void *,void *,void *);
    void *(*held)(void *,void *,void *,int *);
    void *(*loaded)(void *,void *,void *,int *);
    int (*pin)(void *,void *,void *);
    int (*switch_container)(void *,void *,void *);
} VCAPI;
typedef struct {VCRef world,actor,bag;int active,selected,owned,ready,dirty,quarantined;} VCLink;
static void *vc_get(const VCAPI *api,VCRef root){return root?api->get(api->user,root):0;}
static void vc_drop(const VCAPI *api,VCRef *root){if(*root)api->drop(api->user,*root);*root=0;}
static int vc_scope(const VCLink *link,const VCAPI *api,void *world,void *actor) {
    return world&&actor&&vc_get(api,link->world)==world&&
        api->same(api->user,vc_get(api,link->actor),actor);
}
static int vc_bag_valid(const VCLink *link,const VCAPI *api) {
    void *world=vc_get(api,link->world),*actor=vc_get(api,link->actor),*bag=vc_get(api,link->bag);
    if(!bag)return 0;
    int life=api->alive(api->user,world);if(life!=1)return life<0?-1:0;
    return api->valid_bag(api->user,world,actor,bag);
}
static void *vc_resolve(const VCLink *link,const VCAPI *api,void *world,void *actor,int vacuum_held) {
    if(link->quarantined||!vacuum_held||!link->active||!link->selected||!link->owned||
       !vc_scope(link,api,world,actor)||vc_bag_valid(link,api)!=1)return 0;
    return vc_get(api,link->bag);
}
static int vc_detach(VCLink *link,const VCAPI *api) {
    link->ready=0;
    if(link->quarantined)return 0;
    if(!link->active&&!link->dirty)return 1;
    void *world=vc_get(api,link->world),*actor=vc_get(api,link->actor);
    int life=api->alive(api->user,world);if(life<0)return 0;
    if(!life){link->active=link->dirty=0;return 1;}
    int ok=1;
    if(vc_bag_valid(link,api)<0)return 0;
    (void)api->loaded(api->user,world,actor,&ok);if(!ok)return 0;
    /* Pin while the old identity is still active, then let native Switch save
     * that mass before loading the newly selected real tool. */
    if(!api->pin(api->user,world,actor)){link->quarantined=1;return 0;}
    link->active=0;link->dirty=1;
    if(!api->switch_container(api->user,world,actor)){link->quarantined=1;return 0;}
    void *held=api->held(api->user,world,actor,&ok);
    if(!ok){link->quarantined=1;return 0;}
    void *loaded=api->loaded(api->user,world,actor,&ok);
    if(!ok||!api->same(api->user,loaded,held)){link->quarantined=1;return 0;}
    link->dirty=0;return 1;
}
static void vc_forget(VCLink *link,const VCAPI *api) {
    vc_drop(api,&link->bag);vc_drop(api,&link->actor);vc_drop(api,&link->world);
    link->active=link->selected=link->owned=link->ready=link->dirty=link->quarantined=0;
}
static int vc_update(VCLink *link,const VCAPI *api,void *world,void *actor,int selected,int owned) {
    if(!vc_scope(link,api,world,actor)) {
        /* Never issue old-player/world mutations after ownership of the local
         * context changes. Its native ledger already names the genuine bag. */
        if(!world||!actor||api->alive(api->user,world)!=1){link->ready=0;return 0;}
        vc_forget(link,api);
        link->world=api->hold(api->user,world);link->actor=api->hold(api->user,actor);
        if(!link->world||!link->actor){vc_forget(link,api);return 0;}
    }
    /* Native Switch performs several writes before committing loaded identity.
     * A failed invocation has unknown progress; never retry in this context. */
    if(link->quarantined){link->ready=0;return 0;}
    link->selected=selected;link->owned=owned;link->ready=0;
    if(!owned&&!link->active&&!link->dirty){vc_drop(api,&link->bag);return 0;}
    int ok=1,bag_valid=vc_bag_valid(link,api);
    if(bag_valid<0)return 0;
    void *held=!selected&&owned?api->held(api->user,world,actor,&ok):0;
    if(!ok)return 0;
    int held_valid=held?api->valid_bag(api->user,world,actor,held):0;
    if(held_valid<0)return 0;
    void *loaded=api->loaded(api->user,world,actor,&ok);
    if(!ok)return 0;
    int loaded_valid=selected&&owned&&bag_valid!=1&&loaded?api->valid_bag(api->user,world,actor,loaded):0;
    if(loaded_valid<0)return 0;
    if(!selected||!owned) {
        if(!vc_detach(link,api))return 0;
        if(!owned){vc_drop(api,&link->bag);return 0;}
        if(held_valid==1) {
            VCRef root=api->hold(api->user,held);
            vc_drop(api,&link->bag);link->bag=root;
        }
        return 0;
    }
    if(bag_valid!=1) {
        if(!vc_detach(link,api))return 0;
        vc_drop(api,&link->bag);
        /* A confirmed native loaded identity can recover a reload binding;
         * failed queries must never become evidence that a bag is absent. */
        if(loaded_valid==1)link->bag=api->hold(api->user,loaded);
        if(vc_bag_valid(link,api)!=1)return 0;
    }
    void *bag=vc_get(api,link->bag);
    if(link->dirty||!link->active||!api->same(api->user,loaded,bag)) {
        link->ready=0;
        if(!api->pin(api->user,world,actor)){link->quarantined=1;return 0;}
        link->active=link->dirty=1;
        if(!api->switch_container(api->user,world,actor)){link->quarantined=1;return 0;}
        loaded=api->loaded(api->user,world,actor,&ok);
        /* A successful void call does not prove the requested transition
         * committed. Failed or contradictory readback must never be retried. */
        if(!ok||!api->same(api->user,loaded,bag)||vc_bag_valid(link,api)!=1){link->quarantined=1;return 0;}
        link->dirty=0;
    }
    link->ready=api->same(api->user,loaded,bag)&&vc_bag_valid(link,api)==1;
    return link->ready;
}
#endif
