/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2017, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __GPENCIL_ENGINE_H__
#define __GPENCIL_ENGINE_H__

#include "DNA_gpencil_types.h"

#include "GPU_batch.h"

extern DrawEngineType draw_engine_gpencil_type;

struct GPENCIL_Data;
struct GPENCIL_StorageList;
struct MaterialGPencilStyle;
struct Object;
struct RenderEngine;
struct RenderLayer;
struct bGPDstroke;
struct View3D;

struct GPUBatch;
struct GPUVertBuf;
struct GPUVertFormat;

/* used to convert pixel scale. */
#define GPENCIL_PIXEL_FACTOR 2000.0f

/* used to expand VBOs. Size has a big impact in the speed */
#define GPENCIL_VBO_BLOCK_SIZE 128

#define GP_IS_CAMERAVIEW ((rv3d != NULL) && (rv3d->persp == RV3D_CAMOB && v3d->camera))

/* UBO structure. Watch out for padding. Must match GLSL declaration. */
typedef struct gpMaterial {
  float stroke_color[4];
  float fill_color[4];
  float fill_mix_color[4];
  float fill_uv_transform[3][2], _pad0[2];
  float stroke_texture_mix;
  float stroke_u_scale;
  float fill_texture_mix;
  int flag;
} gpMaterial;

/* gpMaterial->flag */
/* WATCH Keep in sync with GLSL declaration. */
#define GP_STROKE_ALIGNMENT_STROKE 1
#define GP_STROKE_ALIGNMENT_OBJECT 2
#define GP_STROKE_ALIGNMENT_FIXED 3
#define GP_STROKE_ALIGNMENT 0x3
#define GP_STROKE_OVERLAP (1 << 2)
#define GP_STROKE_TEXTURE_USE (1 << 3)
#define GP_STROKE_TEXTURE_STENCIL (1 << 4)
#define GP_STROKE_TEXTURE_PREMUL (1 << 5)
#define GP_STROKE_DOTS (1 << 6)
#define GP_FILL_TEXTURE_USE (1 << 10)
#define GP_FILL_TEXTURE_PREMUL (1 << 11)
#define GP_FILL_TEXTURE_CLIP (1 << 12)
#define GP_FILL_GRADIENT_USE (1 << 13)
#define GP_FILL_GRADIENT_RADIAL (1 << 14)

#define GPENCIL_LIGHT_BUFFER_LEN 128

/* UBO structure. Watch out for padding. Must match GLSL declaration. */
typedef struct gpLight {
  float color[3], type;
  float right[3], spotsize;
  float up[3], spotblend;
  float forward[4];
  float position[4];
} gpLight;

/* gpLight->type */
/* WATCH Keep in sync with GLSL declaration. */
#define GP_LIGHT_TYPE_POINT 0.0
#define GP_LIGHT_TYPE_SPOT 1.0
#define GP_LIGHT_TYPE_SUN 2.0
#define GP_LIGHT_TYPE_AMBIENT 3.0

BLI_STATIC_ASSERT_ALIGN(gpMaterial, 16)
BLI_STATIC_ASSERT_ALIGN(gpLight, 16)

/* *********** Draw Datas *********** */
typedef struct GPENCIL_MaterialPool {
  /* Linklist. */
  struct GPENCIL_MaterialPool *next;
  /* GPU representatin of materials. */
  gpMaterial mat_data[GP_MATERIAL_BUFFER_LEN];
  /* Matching ubo. */
  struct GPUUniformBuffer *ubo;
  /* Texture per material. NULL means none. */
  struct GPUTexture *tex_fill[GP_MATERIAL_BUFFER_LEN];
  struct GPUTexture *tex_stroke[GP_MATERIAL_BUFFER_LEN];
} GPENCIL_MaterialPool;

typedef struct GPENCIL_LightPool {
  /* GPU representatin of materials. */
  gpLight light_data[GPENCIL_LIGHT_BUFFER_LEN];
  /* Matching ubo. */
  struct GPUUniformBuffer *ubo;
  /* Number of light in the pool. */
  int light_used;
} GPENCIL_LightPool;

typedef struct GPENCIL_ViewLayerData {
  /* GPENCIL_tObject */
  struct BLI_memblock *gp_object_pool;
  /* GPENCIL_tLayer */
  struct BLI_memblock *gp_layer_pool;
  /* GPENCIL_tVfx */
  struct BLI_memblock *gp_vfx_pool;
  /* GPENCIL_MaterialPool */
  struct BLI_memblock *gp_material_pool;
  /* GPENCIL_LightPool */
  struct BLI_memblock *gp_light_pool;
} GPENCIL_ViewLayerData;

/* *********** GPencil  *********** */

typedef struct GPENCIL_tVfx {
  /** Linklist */
  struct GPENCIL_tVfx *next;
  DRWPass *vfx_ps;
  /* Framebuffer reference since it may not be allocated yet. */
  GPUFrameBuffer **target_fb;
} GPENCIL_tVfx;

typedef struct GPENCIL_tLayer {
  /** Linklist */
  struct GPENCIL_tLayer *next;
  /** Geometry pass (draw all strokes). */
  DRWPass *geom_ps;
  /** Blend pass to composite onto the target buffer (blends modes). NULL if not needed. */
  DRWPass *blend_ps;
  /** Used to identify which layers are masks and which are masked. */
  bool is_mask;
  bool is_masked;
  bool do_masked_clear;
} GPENCIL_tLayer;

typedef struct GPENCIL_tObject {
  /** Linklist */
  struct GPENCIL_tObject *next;

  struct {
    GPENCIL_tLayer *first, *last;
  } layers;

  struct {
    GPENCIL_tVfx *first, *last;
  } vfx;

  /* Distance to camera. Used for sorting. */
  float camera_z;
  /* Normal used for shading. Based on view angle. */
  float plane_normal[3];
  /* Used for drawing depth merge pass. */
  float plane_mat[4][4];

  bool is_drawmode3d;
} GPENCIL_tObject;

/* *********** LISTS *********** */
typedef struct GPENCIL_Storage {
  /* Render Matrices and data */
  float view_vecs[2][4]; /* vec4[2] */
  Object *camera;        /* camera pointer for render mode */
} GPENCIL_Storage;

typedef struct GPENCIL_StorageList {
  struct GPENCIL_PrivateData *pd;
  /* TODO remove all below. */
  struct GPENCIL_Storage *storage;
} GPENCIL_StorageList;

typedef struct GPENCIL_PassList {
  /* Composite the main GPencil buffer onto the rendered image. */
  struct DRWPass *composite_ps;
  /* Composite the object depth to the default depth buffer to occlude overlays. */
  struct DRWPass *merge_depth_ps;
  /* Anti-Aliasing. */
  struct DRWPass *smaa_edge_ps;
  struct DRWPass *smaa_weight_ps;
  struct DRWPass *smaa_resolve_ps;
} GPENCIL_PassList;

typedef struct GPENCIL_FramebufferList {
  struct GPUFrameBuffer *main;

  /* Refactored */
  struct GPUFrameBuffer *gpencil_fb;
  struct GPUFrameBuffer *snapshot_fb;
  struct GPUFrameBuffer *layer_fb;
  struct GPUFrameBuffer *object_fb;
  struct GPUFrameBuffer *masked_fb;
  struct GPUFrameBuffer *smaa_edge_fb;
  struct GPUFrameBuffer *smaa_weight_fb;
} GPENCIL_FramebufferList;

typedef struct GPENCIL_TextureList {
  /* Dummy texture to avoid errors cause by empty sampler. */
  struct GPUTexture *dummy_texture;
  /* Snapshot for smoother drawing. */
  struct GPUTexture *snapshot_depth_tx;
  struct GPUTexture *snapshot_color_tx;
  struct GPUTexture *snapshot_reveal_tx;
  /* Textures used by Antialiasing. */
  struct GPUTexture *smaa_area_tx;
  struct GPUTexture *smaa_search_tx;

} GPENCIL_TextureList;

typedef struct GPENCIL_Data {
  void *engine_type; /* Required */
  struct GPENCIL_FramebufferList *fbl;
  struct GPENCIL_TextureList *txl;
  struct GPENCIL_PassList *psl;
  struct GPENCIL_StorageList *stl;

  /* render textures */
  struct GPUTexture *render_depth_tx;
  struct GPUTexture *render_color_tx;

} GPENCIL_Data;

/* *********** STATIC *********** */
typedef struct GPENCIL_PrivateData {
  /* Pointers copied from GPENCIL_ViewLayerData. */
  struct BLI_memblock *gp_object_pool;
  struct BLI_memblock *gp_layer_pool;
  struct BLI_memblock *gp_vfx_pool;
  struct BLI_memblock *gp_material_pool;
  struct BLI_memblock *gp_light_pool;
  /* Last used material pool. */
  GPENCIL_MaterialPool *last_material_pool;
  /* Last used light pool. */
  GPENCIL_LightPool *last_light_pool;
  /* Common lightpool containing all lights in the scene. */
  GPENCIL_LightPool *global_light_pool;
  /* Common lightpool containing one ambient white light. */
  GPENCIL_LightPool *shadeless_light_pool;
  /* Linked list of tObjects. */
  struct {
    GPENCIL_tObject *first, *last;
  } tobjects;
  /* Temp Textures (shared with other engines). */
  GPUTexture *depth_tx;
  GPUTexture *color_tx;
  GPUTexture *color_layer_tx;
  GPUTexture *color_object_tx;
  GPUTexture *color_masked_tx;
  /* Revealage is 1 - alpha */
  GPUTexture *reveal_tx;
  GPUTexture *reveal_layer_tx;
  GPUTexture *reveal_object_tx;
  GPUTexture *reveal_masked_tx;
  /* Anti-Aliasing. */
  GPUTexture *smaa_edge_tx;
  GPUTexture *smaa_weight_tx;
  /* Pointer to dtxl->depth */
  GPUTexture *scene_depth_tx;
  /* Current frame */
  int cfra;
  /* If we are rendering for final render (F12). */
  bool is_render;
  /* True in selection and auto_depth drawing */
  bool draw_depth_only;
  /* Used by the depth merge step. */
  int is_stroke_order_3d;
  float object_bound_mat[4][4];
  /* Used for computing object distance to camera. */
  float camera_z_axis[3], camera_z_offset;
  float camera_pos[3];
  /* Pseudo depth of field parameter. Used to scale blur radius. */
  float dof_params[2];
  /* Used for DoF Setup. */
  Object *camera;

  /* Object being in draw mode. */
  struct bGPdata *sbuffer_gpd;
  /* Layer to append the temp stroke to. */
  struct bGPDlayer *sbuffer_layer;
  /* Temporary stroke currently being drawn. */
  struct bGPDstroke *sbuffer_stroke;
  /* List of temp objects containing the stroke. */
  struct {
    GPENCIL_tObject *first, *last;
  } sbuffer_tobjects;
  /* Batches containing the temp stroke. */
  GPUBatch *stroke_batch;
  GPUBatch *fill_batch;
  bool do_fast_drawing;
  bool snapshot_buffer_dirty;

  /* Display onion skinning */
  bool do_onion;

  /* simplify settings*/
  bool simplify_fill;
  bool simplify_fx;

} GPENCIL_PrivateData;

typedef struct GPENCIL_e_data {
  /* textures */
  struct GPUTexture *gpencil_blank_texture;

  /* SMAA antialiasing */
  struct GPUShader *antialiasing_sh[3];
  /* GPencil Object rendering */
  struct GPUShader *gpencil_sh;
  /* Final Compositing over rendered background. */
  struct GPUShader *composite_sh;
  /* All layer blend types in one shader! */
  struct GPUShader *layer_blend_sh;
  /* To blend masked layer with other layers. */
  struct GPUShader *layer_mask_sh;
  /* Merge the final object depth to the depth buffer. */
  struct GPUShader *depth_merge_sh;
  /* Effects. */
  struct GPUShader *fx_composite_sh;
  struct GPUShader *fx_colorize_sh;
  struct GPUShader *fx_blur_sh;
  struct GPUShader *fx_glow_sh;
  struct GPUShader *fx_pixel_sh;
  struct GPUShader *fx_rim_sh;
  struct GPUShader *fx_shadow_sh;
  struct GPUShader *fx_transform_sh;

  /* general drawing shaders */
  struct GPUShader *gpencil_fill_sh;
  struct GPUShader *gpencil_stroke_sh;
  struct GPUShader *gpencil_point_sh;
  struct GPUShader *gpencil_edit_point_sh;
  struct GPUShader *gpencil_line_sh;
  struct GPUShader *gpencil_drawing_fill_sh;
  struct GPUShader *gpencil_fullscreen_sh;
  struct GPUShader *gpencil_simple_fullscreen_sh;
  struct GPUShader *gpencil_blend_fullscreen_sh;
  struct GPUShader *gpencil_background_sh;
  struct GPUShader *gpencil_paper_sh;

  /* Dummy vbos. */
  struct GPUVertBuf *quad;

} GPENCIL_e_data; /* Engine data */

extern GPENCIL_e_data en_data;

/* Runtime data for GPU and evaluated frames after applying modifiers */
typedef struct GpencilBatchCache {
  /** Cache is dirty */
  bool is_dirty;
  /** Edit mode flag */
  bool is_editmode;
  /** Last cache frame */
  int cache_frame;
} GpencilBatchCache;

/* geometry batch cache functions */
struct GpencilBatchCache *gpencil_batch_cache_get(struct Object *ob, int cfra);

GPENCIL_tObject *gpencil_object_cache_add(GPENCIL_PrivateData *pd, Object *ob);
GPENCIL_tLayer *gpencil_layer_cache_add(GPENCIL_PrivateData *pd,
                                        Object *ob,
                                        struct bGPDlayer *layer);
GPENCIL_MaterialPool *gpencil_material_pool_create(GPENCIL_PrivateData *pd, Object *ob, int *ofs);
void gpencil_material_resources_get(GPENCIL_MaterialPool *first_pool,
                                    int mat_id,
                                    struct GPUTexture **r_tex_stroke,
                                    struct GPUTexture **r_tex_fill,
                                    struct GPUUniformBuffer **r_ubo_mat);
/*  Meh, TODO fix naming...*/
void gpencil_light_ambient_add(GPENCIL_LightPool *lightpool, const float color[3]);
void gpencil_light_pool_populate(GPENCIL_LightPool *matpool, Object *ob);
GPENCIL_LightPool *gpencil_light_pool_add(GPENCIL_PrivateData *pd);
GPENCIL_LightPool *gpencil_light_pool_create(GPENCIL_PrivateData *pd, Object *ob);

/* effects */
void gpencil_vfx_cache_populate(GPENCIL_Data *vedata, Object *ob, GPENCIL_tObject *tgp_ob);

/* Shaders */
struct GPUShader *GPENCIL_shader_antialiasing(GPENCIL_e_data *e_data, int stage);
struct GPUShader *GPENCIL_shader_geometry_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_composite_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_layer_blend_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_layer_mask_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_depth_merge_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_blur_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_colorize_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_composite_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_transform_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_glow_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_pixelize_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_rim_get(GPENCIL_e_data *e_data);
struct GPUShader *GPENCIL_shader_fx_shadow_get(GPENCIL_e_data *e_data);

/* Antialiasing */
void GPENCIL_antialiasing_init(struct GPENCIL_Data *vedata);
void GPENCIL_antialiasing_draw(struct GPENCIL_Data *vedata);

/* main functions */
void GPENCIL_engine_init(void *vedata);
void GPENCIL_cache_init(void *vedata);
void GPENCIL_cache_populate(void *vedata, struct Object *ob);
void GPENCIL_cache_finish(void *vedata);
void GPENCIL_draw_scene(void *vedata);

/* render */
void GPENCIL_render_init(struct GPENCIL_Data *ved,
                         struct RenderEngine *engine,
                         struct Depsgraph *depsgraph);
void GPENCIL_render_to_image(void *vedata,
                             struct RenderEngine *engine,
                             struct RenderLayer *render_layer,
                             const rcti *rect);

/* Draw Data. */
void gpencil_light_pool_free(void *storage);
void gpencil_material_pool_free(void *storage);
GPENCIL_ViewLayerData *GPENCIL_view_layer_data_ensure(void);

/* Use of multisample framebuffers. */
#define MULTISAMPLE_GP_SYNC_ENABLE(lvl, fbl) \
  { \
    if ((lvl > 0) && (fbl->multisample_fb != NULL) && (DRW_state_is_fbo())) { \
      DRW_stats_query_start("GP Multisample Blit"); \
      GPU_framebuffer_bind(fbl->multisample_fb); \
      GPU_framebuffer_clear_color_depth_stencil( \
          fbl->multisample_fb, (const float[4]){0.0f}, 1.0f, 0x0); \
      DRW_stats_query_end(); \
    } \
  } \
  ((void)0)

#define MULTISAMPLE_GP_SYNC_DISABLE(lvl, fbl, fb, txl) \
  { \
    if ((lvl > 0) && (fbl->multisample_fb != NULL) && (DRW_state_is_fbo())) { \
      DRW_stats_query_start("GP Multisample Resolve"); \
      GPU_framebuffer_bind(fb); \
      DRW_multisamples_resolve(txl->multisample_depth, txl->multisample_color, true); \
      DRW_stats_query_end(); \
    } \
  } \
  ((void)0)

#define GPENCIL_3D_DRAWMODE(ob, gpd) \
  ((gpd) && (gpd->draw_mode == GP_DRAWMODE_3D) && ((ob->dtx & OB_DRAWXRAY) == 0))

#define GPENCIL_USE_SOLID(stl) \
  ((stl) && ((stl->storage->is_render) || (stl->storage->is_mat_preview)))

#endif /* __GPENCIL_ENGINE_H__ */
