#include "scene.h"
#include "graphics.h"

#include <math.h>
#include <cairo/cairo.h>

struct Scene {
    tlLock lock;
    tlTask* rendertask;
    Graphics* graphics;

    int width;
    int height;
    Node* root;

    tlHandle onkey;
    tlHandle onresize;

    SceneDirtySignalCb* dirty_signal;
    void* dirty_data;
};

struct Node {
    tlHead head;
    Scene* scene;
    Node* up;
    Node* prev;
    Node* next;
    Node* nodes;

    int x; int y; int width; int height;
    int cx; int cy; int r; // center, rotate
    int sx; int sy;        // scale
    int alpha;

    tlHandle ondraw;
    tlHandle onclick;
};

static tlKind _SceneKind = { .name = "Scene", .locked = true, };
tlKind* SceneKind = &_SceneKind;
static tlKind _NodeKind = { .name = "Node", };
tlKind* NodeKind = &_NodeKind;

static void render_node(cairo_t* c, Node* n, Graphics* g) {
    if (!n) return;

    cairo_save(c);

    // setup transform and clip
    cairo_translate(c, n->x + n->cx, n->y + n->cy);
    if (n->r) cairo_rotate(c, n->r / 360.0);
    cairo_translate(c, -n->cx, -n->cy);
    //if (n->sx != 1 || n->sy != 1) cairo_scale(c, n->sx, n->sy);
    cairo_rectangle(c, 0, 0, n->width, n->height);
    cairo_clip(c);

    // call out to render task, it runs on the vm thread, blocking ours until done
    if (n->ondraw) {
        tlBlockingTaskEval(n->scene->rendertask, tlCallFrom(n->ondraw, g, null));
    }

    // draw the subnodes nodes
    for (Node* sn = n->nodes; sn; sn = sn->next) render_node(c, sn, g);

    cairo_reset_clip(c);
    cairo_restore(c);
}

void sceneRender(Scene* scene, cairo_t* cairo) {
    Graphics* g = GraphicsNew(cairo);
    render_node(cairo, scene->root, g);
}

Scene* SceneNew(int width, int height) {
    Scene* scene = tlAlloc(SceneKind, sizeof(Scene));
    scene->width = width;
    scene->height = height;
    scene->rendertask = tlBlockingTaskNew(tlVmCurrent());
    return scene;
}

void sceneSetDirtySignal(Scene* scene, SceneDirtySignalCb cb, void* data) {
    scene->dirty_signal = cb;
    scene->dirty_data = data;
}

Node* NodeNew(Scene* scene) {
    Node* node = tlAlloc(NodeKind, sizeof(Node));
    node->scene = scene;
    return node;
}

static tlHandle _Scene_new(tlArgs* args) {
    return SceneNew(0, 0);
}

static tlHandle _scene_newNode(tlArgs* args) {
    Node* node = NodeNew(tlArgsTarget(args));
    return node;
}
static tlHandle _scene_add(tlArgs* args) {
    Scene* scene = SceneAs(tlArgsTarget(args));
    Node* node = NodeCast(tlArgsGet(args, 0));
    if (!node) TL_THROW("expect a Node to add");
    if (scene->dirty_signal) scene->dirty_signal(scene->dirty_data);

    if (!scene->root) {
        scene->root = node;
        return node;
    }
    Node* n = scene->root;
    while (n->next) n = n->next;
    n->next = node;
    node->prev = n;
    return node;
}
static tlHandle _scene_remove(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _scene_get(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _scene_up(tlArgs* args) {
    return tlNull;
}
static tlHandle _scene_width(tlArgs* args) {
    Scene* scene = SceneAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) scene->width = tl_int_or(tlArgsGet(args, 0), 0);
    return tlINT(scene->width);
}
static tlHandle _scene_height(tlArgs* args) {
    Scene* scene = SceneAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) scene->height = tl_int_or(tlArgsGet(args, 0), 0);
    return tlINT(scene->height);
}

static tlHandle _node_add(tlArgs* args) {
    Node* up = NodeAs(tlArgsTarget(args));
    Node* node = NodeCast(tlArgsGet(args, 0));
    if (!node) TL_THROW("expect a Node to add");

    if (!up->nodes) {
        up->nodes = node;
        return node;
    }
    Node* n = up->nodes;
    while (n->next) n = n->next;
    n->next = node;
    node->prev = n;
    return node;
}
static tlHandle _node_remove(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _node_get(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _node_up(tlArgs* args) {
    return tlNull;
}
static tlHandle _node_x(tlArgs* args) {
    Node* node = NodeAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) node->x = tl_int_or(tlArgsGet(args, 0), 0);
    return tlINT(node->x);
}
static tlHandle _node_y(tlArgs* args) {
    Node* node = NodeAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) node->y = tl_int_or(tlArgsGet(args, 0), 0);
    return tlINT(node->y);
}
static tlHandle _node_width(tlArgs* args) {
    Node* node = NodeAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) node->width = tl_int_or(tlArgsGet(args, 0), 0);
    return tlINT(node->width);
}
static tlHandle _node_height(tlArgs* args) {
    Node* node = NodeAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) node->height = tl_int_or(tlArgsGet(args, 0), 0);
    return tlINT(node->height);
}
static tlHandle _node_ondraw(tlArgs* args) {
    Node* node = NodeAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) node->ondraw = tlArgsGet(args, 0);
    return node->ondraw?node->ondraw : tlNull;
}

void scene_init(tlVm* vm) {
    _SceneKind.klass = tlClassMapFrom(
        "newNode", _scene_newNode,

        "add", _scene_add,
        "remove", _scene_remove,
        "get", _scene_get,
        "up", _scene_up,

        "width", _scene_width,
        "height", _scene_height,
        null
    );

    _NodeKind.klass = tlClassMapFrom(
        "add", _node_add,
        "remove", _node_remove,
        "get", _node_get,
        "up", _node_up,

        "x", _node_x,
        "y", _node_x,
        "width", _node_width,
        "height", _node_height,

        "ondraw", _node_ondraw,
        null
    );

    tlMap* SceneStatic = tlClassMapFrom(
        "new", _Scene_new,
        null
    );

    tlVmGlobalSet(vm, tlSYM("Scene"), SceneStatic);
}

