#include "vm/tl.h"
#include <cairo/cairo.h>

TL_REF_TYPE(Scene);
TL_REF_TYPE(Node);

void scene_init(tlVm* vm);
Scene* SceneNew(int width, int height);

typedef void(SceneDirtySignalCb)(void*);
void sceneSetDirtySignal(Scene* scene, SceneDirtySignalCb, void* data);
void sceneRender(Scene* scene, cairo_t* cairo);

