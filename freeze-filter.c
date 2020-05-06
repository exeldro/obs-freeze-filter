#include <obs-module.h>
#include "freeze-filter.h"

struct freeze_info {
	obs_source_t *source;
	gs_texrender_t *render;

	uint64_t ts;
	uint32_t cx;
	uint32_t cy;
	bool target_valid;
	bool processed_frame;
	obs_hotkey_pair_id hotkey;
	float duration;
	uint64_t duration_max;
};

static const char *freeze_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("Freeze");
}

static void free_textures(struct freeze_info *f)
{
	if (!f->render)
		return;
	obs_enter_graphics();
	gs_texrender_destroy(f->render);
	f->render = NULL;
	obs_leave_graphics();
}

static inline bool check_size(struct freeze_info *f)
{
	obs_source_t *target = obs_filter_get_target(f->source);

	f->target_valid = !!target;
	if (!f->target_valid)
		return true;

	const uint32_t cx = obs_source_get_base_width(target);
	const uint32_t cy = obs_source_get_base_height(target);

	f->target_valid = !!cx && !!cy;
	if (!f->target_valid)
		return true;

	if (cx != f->cx || cy != f->cy) {
		f->cx = cx;
		f->cy = cy;
		free_textures(f);
		return true;
	}
	return false;
}

static void *freeze_create(obs_data_t *settings, obs_source_t *source)
{
	struct freeze_info *freeze = bzalloc(sizeof(struct freeze_info));
	freeze->source = source;
	freeze->hotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	obs_source_update(source, settings);
	return freeze;
}

static void freeze_destroy(void *data)
{
	struct freeze_info *freeze = data;
	if (freeze->hotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_hotkey_pair_unregister(freeze->hotkey);
	}
	free_textures(freeze);
	bfree(freeze);
}

static void freeze_update(void *data, obs_data_t *settings)
{
	struct freeze_info *freeze = data;
	freeze->duration_max = obs_data_get_int(settings, "duration");
	freeze->cx = 0;
	freeze->cy = 0;
	free_textures(freeze);
}

static void draw_frame(struct freeze_info *f)
{

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_texture_t *tex = gs_texrender_get_texture(f->render);
	if (tex) {
		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, tex);

		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(tex, 0, f->cx, f->cy);
	}
}

static void freeze_video_render(void *data, gs_effect_t *effect)
{
	struct freeze_info *freeze = data;
	obs_source_t *target = obs_filter_get_target(freeze->source);
	obs_source_t *parent = obs_filter_get_parent(freeze->source);

	if (!freeze->target_valid || !target || !parent) {
		obs_source_skip_video_filter(freeze->source);
		return;
	}
	if (freeze->processed_frame) {
		draw_frame(freeze);
		return;
	}
	if (!freeze->render) {
		freeze->render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	} else {
		gs_texrender_reset(freeze->render);
	}

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(freeze->render, freeze->cx, freeze->cy)) {
		uint32_t parent_flags = obs_source_get_output_flags(target);
		bool custom_draw = (parent_flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
		bool async = (parent_flags & OBS_SOURCE_ASYNC) != 0;
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)freeze->cx, 0.0f, (float)freeze->cy,
			 -100.0f, 100.0f);

		if (target == parent && !custom_draw && !async)
			obs_source_default_render(target);
		else
			obs_source_video_render(target);

		gs_texrender_end(freeze->render);
	}

	gs_blend_state_pop();
	draw_frame(freeze);
	freeze->processed_frame = true;
}

static obs_properties_t *freeze_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *p = obs_properties_add_int(
		ppts, "duration", obs_module_text("Duration"), 0, 100000, 1000);
	obs_property_int_set_suffix(p, "ms");
	return ppts;
}

void freeze_defaults(obs_data_t *settings) {}

bool freeze_enable_hotkey(void *data, obs_hotkey_pair_id id,
			  obs_hotkey_t *hotkey, bool pressed)
{
	struct freeze_info *freeze = data;
	if (!pressed)
		return false;

	if (obs_source_enabled(freeze->source))
		return false;

	obs_source_set_enabled(freeze->source, true);

	return true;
}

bool freeze_disable_hotkey(void *data, obs_hotkey_pair_id id,
			   obs_hotkey_t *hotkey, bool pressed)
{
	struct freeze_info *freeze = data;
	if (!pressed)
		return false;
	if (!obs_source_enabled(freeze->source))
		return false;

	obs_source_set_enabled(freeze->source, false);
	return true;
}

static void freeze_tick(void *data, float t)
{

	struct freeze_info *f = data;
	if (obs_source_enabled(f->source)) {
		f->duration += t;
		if (f->duration_max && f->duration * 1000.0 > f->duration_max) {
			obs_source_set_enabled(f->source, false);
		}
	} else {
		f->processed_frame = false;
		f->duration = 0.0f;
	}
	if (f->hotkey == OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_source_t *parent = obs_filter_get_parent(f->source);
		if (parent) {
			f->hotkey = obs_hotkey_pair_register_source(
				parent, "Freeze.Enable",
				obs_module_text("FreezeEnable"),
				"Freeze.Disable",
				obs_module_text("FreezeDisable"),
				freeze_enable_hotkey, freeze_disable_hotkey, f,
				f);
		}
	}
	check_size(f);
}

struct obs_source_info freeze_filter = {
	.id = "freeze_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_OUTPUT_VIDEO,
	.get_name = freeze_get_name,
	.create = freeze_create,
	.destroy = freeze_destroy,
	.update = freeze_update,
	.video_render = freeze_video_render,
	.get_properties = freeze_properties,
	.get_defaults = freeze_defaults,
	.video_tick = freeze_tick,
};

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("freeze-filter", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("FreezeFilter");
}

bool obs_module_load(void)
{
	obs_register_source(&freeze_filter);
	return true;
}
