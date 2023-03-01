/**************************************************************************/
/*  webxr_interface_js.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef WEB_ENABLED

#include "webxr_interface_js.h"

#include "core/input/input.h"
#include "core/os/os.h"
#include "drivers/gles3/storage/texture_storage.h"
#include "emscripten.h"
#include "godot_webxr.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/rendering_server_globals.h"

#include <stdlib.h>

void _emwebxr_on_session_supported(char *p_session_mode, int p_supported) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	Ref<XRInterface> interface = xr_server->find_interface("WebXR");
	ERR_FAIL_COND(interface.is_null());

	String session_mode = String(p_session_mode);
	interface->emit_signal(SNAME("session_supported"), session_mode, p_supported ? true : false);
}

void _emwebxr_on_session_started(char *p_reference_space_type) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	Ref<XRInterface> interface = xr_server->find_interface("WebXR");
	ERR_FAIL_COND(interface.is_null());

	String reference_space_type = String(p_reference_space_type);
	static_cast<WebXRInterfaceJS *>(interface.ptr())->_set_reference_space_type(reference_space_type);
	interface->emit_signal(SNAME("session_started"));
}

void _emwebxr_on_session_ended() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	Ref<XRInterface> interface = xr_server->find_interface("WebXR");
	ERR_FAIL_COND(interface.is_null());

	interface->uninitialize();
	interface->emit_signal(SNAME("session_ended"));
}

void _emwebxr_on_session_failed(char *p_message) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	Ref<XRInterface> interface = xr_server->find_interface("WebXR");
	ERR_FAIL_COND(interface.is_null());

	interface->uninitialize();

	String message = String(p_message);
	interface->emit_signal(SNAME("session_failed"), message);
}

extern "C" EMSCRIPTEN_KEEPALIVE void _emwebxr_on_input_event(int p_event_type, int p_input_source_id) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	Ref<XRInterface> interface = xr_server->find_interface("WebXR");
	ERR_FAIL_COND(interface.is_null());

	((WebXRInterfaceJS *)interface.ptr())->_on_input_event(p_event_type, p_input_source_id);
}

extern "C" EMSCRIPTEN_KEEPALIVE void _emwebxr_on_simple_event(char *p_signal_name) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	Ref<XRInterface> interface = xr_server->find_interface("WebXR");
	ERR_FAIL_COND(interface.is_null());

	StringName signal_name = StringName(p_signal_name);
	interface->emit_signal(signal_name);
}

void WebXRInterfaceJS::is_session_supported(const String &p_session_mode) {
	godot_webxr_is_session_supported(p_session_mode.utf8().get_data(), &_emwebxr_on_session_supported);
}

void WebXRInterfaceJS::set_session_mode(String p_session_mode) {
	session_mode = p_session_mode;
}

String WebXRInterfaceJS::get_session_mode() const {
	return session_mode;
}

void WebXRInterfaceJS::set_required_features(String p_required_features) {
	required_features = p_required_features;
}

String WebXRInterfaceJS::get_required_features() const {
	return required_features;
}

void WebXRInterfaceJS::set_optional_features(String p_optional_features) {
	optional_features = p_optional_features;
}

String WebXRInterfaceJS::get_optional_features() const {
	return optional_features;
}

void WebXRInterfaceJS::set_requested_reference_space_types(String p_requested_reference_space_types) {
	requested_reference_space_types = p_requested_reference_space_types;
}

String WebXRInterfaceJS::get_requested_reference_space_types() const {
	return requested_reference_space_types;
}

void WebXRInterfaceJS::_set_reference_space_type(String p_reference_space_type) {
	reference_space_type = p_reference_space_type;
}

String WebXRInterfaceJS::get_reference_space_type() const {
	return reference_space_type;
}

bool WebXRInterfaceJS::is_input_source_active(int p_input_source_id) const {
	ERR_FAIL_INDEX_V(p_input_source_id, input_source_count, false);
	return input_sources[p_input_source_id].active;
}

Ref<XRPositionalTracker> WebXRInterfaceJS::get_input_source_tracker(int p_input_source_id) const {
	ERR_FAIL_INDEX_V(p_input_source_id, input_source_count, Ref<XRPositionalTracker>());
	return input_sources[p_input_source_id].tracker;
}

WebXRInterface::TargetRayMode WebXRInterfaceJS::get_input_source_target_ray_mode(int p_input_source_id) const {
	ERR_FAIL_INDEX_V(p_input_source_id, input_source_count, WebXRInterface::TARGET_RAY_MODE_UNKNOWN);
	if (!input_sources[p_input_source_id].active) {
		return WebXRInterface::TARGET_RAY_MODE_UNKNOWN;
	}
	return input_sources[p_input_source_id].target_ray_mode;
}

String WebXRInterfaceJS::get_visibility_state() const {
	char *c_str = godot_webxr_get_visibility_state();
	if (c_str) {
		String visibility_state = String(c_str);
		free(c_str);

		return visibility_state;
	}
	return String();
}

PackedVector3Array WebXRInterfaceJS::get_play_area() const {
	PackedVector3Array ret;

	float *points;
	int point_count = godot_webxr_get_bounds_geometry(&points);
	if (point_count > 0) {
		ret.resize(point_count);
		for (int i = 0; i < point_count; i++) {
			float *js_vector3 = points + (i * 3);
			ret.set(i, Vector3(js_vector3[0], js_vector3[1], js_vector3[2]));
		}
		free(points);
	}

	return ret;
}

StringName WebXRInterfaceJS::get_name() const {
	return "WebXR";
};

uint32_t WebXRInterfaceJS::get_capabilities() const {
	return XRInterface::XR_STEREO | XRInterface::XR_MONO | XRInterface::XR_VR | XRInterface::XR_AR;
};

uint32_t WebXRInterfaceJS::get_view_count() {
	return godot_webxr_get_view_count();
};

bool WebXRInterfaceJS::is_initialized() const {
	return (initialized);
};

bool WebXRInterfaceJS::initialize() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, false);

	if (!initialized) {
		if (!godot_webxr_is_supported()) {
			return false;
		}

		if (requested_reference_space_types.size() == 0) {
			return false;
		}

		// we must create a tracker for our head
		head_transform.basis = Basis();
		head_transform.origin = Vector3();
		head_tracker.instantiate();
		head_tracker->set_tracker_type(XRServer::TRACKER_HEAD);
		head_tracker->set_tracker_name("head");
		head_tracker->set_tracker_desc("Players head");
		xr_server->add_tracker(head_tracker);

		// make this our primary interface
		xr_server->set_primary_interface(this);

		// Clear render_targetsize to make sure it gets reset to the new size.
		// Clearing in uninitialize() doesn't work because a frame can still be
		// rendered after it's called, which will fill render_targetsize again.
		render_targetsize.width = 0;
		render_targetsize.height = 0;

		initialized = true;

		godot_webxr_initialize(
				session_mode.utf8().get_data(),
				required_features.utf8().get_data(),
				optional_features.utf8().get_data(),
				requested_reference_space_types.utf8().get_data(),
				&_emwebxr_on_session_started,
				&_emwebxr_on_session_ended,
				&_emwebxr_on_session_failed,
				&_emwebxr_on_input_event,
				&_emwebxr_on_simple_event);
	};

	return true;
};

void WebXRInterfaceJS::uninitialize() {
	if (initialized) {
		XRServer *xr_server = XRServer::get_singleton();
		if (xr_server != nullptr) {
			if (head_tracker.is_valid()) {
				xr_server->remove_tracker(head_tracker);

				head_tracker.unref();
			}

			if (xr_server->get_primary_interface() == this) {
				// no longer our primary interface
				xr_server->set_primary_interface(nullptr);
			}
		}

		godot_webxr_uninitialize();

		GLES3::TextureStorage *texture_storage = dynamic_cast<GLES3::TextureStorage *>(RSG::texture_storage);
		if (texture_storage != nullptr) {
			for (KeyValue<unsigned int, RID> &E : texture_cache) {
				// Forcibly mark as not part of a render target so we can free it.
				GLES3::Texture *texture = texture_storage->get_texture(E.value);
				texture->is_render_target = false;

				texture_storage->texture_free(E.value);
			}
		}

		texture_cache.clear();
		reference_space_type = "";
		initialized = false;
	};
};

Transform3D WebXRInterfaceJS::_js_matrix_to_transform(float *p_js_matrix) {
	Transform3D transform;

	transform.basis.rows[0].x = p_js_matrix[0];
	transform.basis.rows[1].x = p_js_matrix[1];
	transform.basis.rows[2].x = p_js_matrix[2];
	transform.basis.rows[0].y = p_js_matrix[4];
	transform.basis.rows[1].y = p_js_matrix[5];
	transform.basis.rows[2].y = p_js_matrix[6];
	transform.basis.rows[0].z = p_js_matrix[8];
	transform.basis.rows[1].z = p_js_matrix[9];
	transform.basis.rows[2].z = p_js_matrix[10];
	transform.origin.x = p_js_matrix[12];
	transform.origin.y = p_js_matrix[13];
	transform.origin.z = p_js_matrix[14];

	return transform;
}

Size2 WebXRInterfaceJS::get_render_target_size() {
	if (render_targetsize.width != 0 && render_targetsize.height != 0) {
		return render_targetsize;
	}

	int js_size[2];
	bool has_size = godot_webxr_get_render_target_size(js_size);

	if (!initialized || !has_size) {
		// As a temporary default (until WebXR is fully initialized), use the
		// window size.
		return DisplayServer::get_singleton()->window_get_size();
	}

	render_targetsize.width = (float)js_size[0];
	render_targetsize.height = (float)js_size[1];

	return render_targetsize;
};

Transform3D WebXRInterfaceJS::get_camera_transform() {
	Transform3D camera_transform;

	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, camera_transform);

	if (initialized) {
		double world_scale = xr_server->get_world_scale();

		Transform3D _head_transform = head_transform;
		_head_transform.origin *= world_scale;

		camera_transform = (xr_server->get_reference_frame()) * _head_transform;
	}

	return camera_transform;
};

Transform3D WebXRInterfaceJS::get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, p_cam_transform);
	ERR_FAIL_COND_V(!initialized, p_cam_transform);

	float js_matrix[16];
	bool has_transform = godot_webxr_get_transform_for_view(p_view, js_matrix);
	if (!has_transform) {
		return p_cam_transform;
	}

	Transform3D transform_for_view = _js_matrix_to_transform(js_matrix);

	double world_scale = xr_server->get_world_scale();
	transform_for_view.origin *= world_scale;

	return p_cam_transform * xr_server->get_reference_frame() * transform_for_view;
};

Projection WebXRInterfaceJS::get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) {
	Projection view;

	ERR_FAIL_COND_V(!initialized, view);

	float js_matrix[16];
	bool has_projection = godot_webxr_get_projection_for_view(p_view, js_matrix);
	if (!has_projection) {
		return view;
	}

	int k = 0;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			view.columns[i][j] = js_matrix[k++];
		}
	}

	// Copied from godot_oculus_mobile's ovr_mobile_session.cpp
	view.columns[2][2] = -(p_z_far + p_z_near) / (p_z_far - p_z_near);
	view.columns[3][2] = -(2.0f * p_z_far * p_z_near) / (p_z_far - p_z_near);

	return view;
}

bool WebXRInterfaceJS::pre_draw_viewport(RID p_render_target) {
	GLES3::TextureStorage *texture_storage = dynamic_cast<GLES3::TextureStorage *>(RSG::texture_storage);
	if (texture_storage == nullptr) {
		return false;
	}

	GLES3::RenderTarget *rt = texture_storage->get_render_target(p_render_target);
	if (rt == nullptr) {
		return false;
	}

	// Cache the resources so we don't have to get them from JS twice.
	color_texture = _get_color_texture();
	depth_texture = _get_depth_texture();

	// Per the WebXR spec, it returns "opaque textures" to us, which may be the
	// same WebGLTexture object (which would be the same GLuint in C++) but
	// represent a different underlying resource (probably the next texture in
	// the XR device's swap chain). In order to render to this texture, we need
	// to re-attach it to the FBO, otherwise we get an "incomplete FBO" error.
	//
	// See: https://immersive-web.github.io/layers/#xropaquetextures
	//
	// This is why we're doing this sort of silly check: if the color and depth
	// textures are the same this frame as last frame, we need to attach them
	// again, despite the fact that the GLuint for them hasn't changed.
	if (rt->overridden.is_overridden && rt->overridden.color == color_texture && rt->overridden.depth == depth_texture) {
		GLES3::Config *config = GLES3::Config::get_singleton();
		bool use_multiview = rt->view_count > 1 && config->multiview_supported;

		glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
		if (use_multiview) {
			glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt->color, 0, 0, rt->view_count);
			glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rt->depth, 0, 0, rt->view_count);
		} else {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, texture_storage->system_fbo);
	}

	return true;
}

Vector<BlitToScreen> WebXRInterfaceJS::post_draw_viewport(RID p_render_target, const Rect2 &p_screen_rect) {
	Vector<BlitToScreen> blit_to_screen;

	// We don't need to do anything here.

	return blit_to_screen;
};

RID WebXRInterfaceJS::_get_color_texture() {
	unsigned int texture_id = godot_webxr_get_color_texture();
	if (texture_id == 0) {
		return RID();
	}

	return _get_texture(texture_id);
}

RID WebXRInterfaceJS::_get_depth_texture() {
	unsigned int texture_id = godot_webxr_get_depth_texture();
	if (texture_id == 0) {
		return RID();
	}

	return _get_texture(texture_id);
}

RID WebXRInterfaceJS::_get_texture(unsigned int p_texture_id) {
	RBMap<unsigned int, RID>::Element *cache = texture_cache.find(p_texture_id);
	if (cache != nullptr) {
		return cache->get();
	}

	GLES3::TextureStorage *texture_storage = dynamic_cast<GLES3::TextureStorage *>(RSG::texture_storage);
	if (texture_storage == nullptr) {
		return RID();
	}

	uint32_t view_count = godot_webxr_get_view_count();
	Size2 texture_size = get_render_target_size();

	RID texture = texture_storage->texture_create_external(
			view_count == 1 ? GLES3::Texture::TYPE_2D : GLES3::Texture::TYPE_LAYERED,
			Image::FORMAT_RGBA8,
			p_texture_id,
			(int)texture_size.width,
			(int)texture_size.height,
			1,
			view_count);

	texture_cache.insert(p_texture_id, texture);

	return texture;
}

RID WebXRInterfaceJS::get_color_texture() {
	return color_texture;
}

RID WebXRInterfaceJS::get_depth_texture() {
	return depth_texture;
}

RID WebXRInterfaceJS::get_velocity_texture() {
	unsigned int texture_id = godot_webxr_get_velocity_texture();
	if (texture_id == 0) {
		return RID();
	}

	return _get_texture(texture_id);
}

void WebXRInterfaceJS::process() {
	if (initialized) {
		// Get the "head" position.
		float js_matrix[16];
		if (godot_webxr_get_transform_for_view(-1, js_matrix)) {
			head_transform = _js_matrix_to_transform(js_matrix);
		}
		if (head_tracker.is_valid()) {
			head_tracker->set_pose("default", head_transform, Vector3(), Vector3());
		}

		// Update all input sources.
		for (int i = 0; i < input_source_count; i++) {
			_update_input_source(i);
		}
	};
};

void WebXRInterfaceJS::_update_input_source(int p_input_source_id) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	InputSource &input_source = input_sources[p_input_source_id];

	float target_pose[16];
	int tmp_target_ray_mode;
	int touch_index;
	int has_grip_pose;
	float grip_pose[16];
	int has_standard_mapping;
	int button_count;
	float buttons[10];
	int axes_count;
	float axes[10];

	input_source.active = godot_webxr_update_input_source(
			p_input_source_id,
			target_pose,
			&tmp_target_ray_mode,
			&touch_index,
			&has_grip_pose,
			grip_pose,
			&has_standard_mapping,
			&button_count,
			buttons,
			&axes_count,
			axes);

	if (!input_source.active) {
		if (input_source.tracker.is_valid()) {
			xr_server->remove_tracker(input_source.tracker);
			input_source.tracker.unref();
		}
		return;
	}

	input_source.target_ray_mode = (WebXRInterface::TargetRayMode)tmp_target_ray_mode;
	input_source.touch_index = touch_index;

	Ref<XRPositionalTracker> &tracker = input_source.tracker;

	if (tracker.is_null()) {
		tracker.instantiate();

		StringName tracker_name;
		if (input_source.target_ray_mode == WebXRInterface::TargetRayMode::TARGET_RAY_MODE_SCREEN) {
			tracker_name = touch_names[touch_index];
		} else {
			tracker_name = tracker_names[p_input_source_id];
		}

		// Input source id's 0 and 1 are always the left and right hands.
		if (p_input_source_id < 2) {
			tracker->set_tracker_type(XRServer::TRACKER_CONTROLLER);
			tracker->set_tracker_name(tracker_name);
			tracker->set_tracker_desc(p_input_source_id == 0 ? "Left hand controller" : "Right hand controller");
			tracker->set_tracker_hand(p_input_source_id == 0 ? XRPositionalTracker::TRACKER_HAND_LEFT : XRPositionalTracker::TRACKER_HAND_RIGHT);
		} else {
			tracker->set_tracker_name(tracker_name);
			tracker->set_tracker_desc(tracker_name);
		}
		xr_server->add_tracker(tracker);
	}

	Transform3D aim_transform = _js_matrix_to_transform(target_pose);
	tracker->set_pose(SNAME("default"), aim_transform, Vector3(), Vector3());
	tracker->set_pose(SNAME("aim"), aim_transform, Vector3(), Vector3());
	if (has_grip_pose) {
		tracker->set_pose(SNAME("grip"), _js_matrix_to_transform(grip_pose), Vector3(), Vector3());
	}

	for (int i = 0; i < button_count; i++) {
		StringName button_name = has_standard_mapping ? standard_button_names[i] : unknown_button_names[i];
		StringName button_pressure_name = has_standard_mapping ? standard_button_pressure_names[i] : unknown_button_pressure_names[i];
		float value = buttons[i];
		bool state = value > 0.0;
		tracker->set_input(button_name, state);
		tracker->set_input(button_pressure_name, value);
	}

	for (int i = 0; i < axes_count; i++) {
		StringName axis_name = has_standard_mapping ? standard_axis_names[i] : unknown_axis_names[i];
		float value = axes[i];
		if (has_standard_mapping && (i == 1 || i == 3)) {
			// Invert the Y-axis on thumbsticks and trackpads, in order to
			// match OpenXR and other XR platform SDKs.
			value = -value;
		}
		tracker->set_input(axis_name, value);
	}

	// Also create Vector2's for the thumbstick and trackpad when we have the
	// standard mapping.
	if (has_standard_mapping) {
		if (axes_count >= 2) {
			tracker->set_input(standard_vector_names[0], Vector2(axes[0], -axes[1]));
		}
		if (axes_count >= 4) {
			tracker->set_input(standard_vector_names[1], Vector2(axes[2], -axes[3]));
		}
	}

	if (input_source.target_ray_mode == WebXRInterface::TARGET_RAY_MODE_SCREEN) {
		if (touch_index < 5 && axes_count >= 2) {
			Vector2 joy_vector = Vector2(axes[0], axes[1]);
			Vector2 position = _get_screen_position_from_joy_vector(joy_vector);

			if (touches[touch_index].is_touching) {
				Vector2 delta = position - touches[touch_index].position;

				// If position has changed by at least 1 pixel, generate a drag event.
				if (abs(delta.x) >= 1.0 || abs(delta.y) >= 1.0) {
					Ref<InputEventScreenDrag> event;
					event.instantiate();
					event->set_index(touch_index);
					event->set_position(position);
					event->set_relative(delta);
					Input::get_singleton()->parse_input_event(event);
				}
			}

			touches[touch_index].position = position;
		}
	}
}

void WebXRInterfaceJS::_on_input_event(int p_event_type, int p_input_source_id) {
	// Get the latest data for this input source. For transient input sources,
	// we may not have any data at all yet!
	_update_input_source(p_input_source_id);

	if (p_event_type == WEBXR_INPUT_EVENT_SELECTSTART || p_event_type == WEBXR_INPUT_EVENT_SELECTEND) {
		const InputSource &input_source = input_sources[p_input_source_id];
		if (input_source.target_ray_mode == WebXRInterface::TARGET_RAY_MODE_SCREEN) {
			int touch_index = input_source.touch_index;
			if (touch_index >= 0 && touch_index < 5) {
				touches[touch_index].is_touching = (p_event_type == WEBXR_INPUT_EVENT_SELECTSTART);

				Ref<InputEventScreenTouch> event;
				event.instantiate();
				event->set_index(touch_index);
				event->set_position(touches[touch_index].position);
				event->set_pressed(p_event_type == WEBXR_INPUT_EVENT_SELECTSTART);

				Input::get_singleton()->parse_input_event(event);
			}
		}
	}

	switch (p_event_type) {
		case WEBXR_INPUT_EVENT_SELECTSTART:
			emit_signal("selectstart", p_input_source_id);
			break;

		case WEBXR_INPUT_EVENT_SELECTEND:
			emit_signal("selectend", p_input_source_id);
			// Emit the 'select' event on our own (rather than intercepting the
			// one from JavaScript) so that we don't have to needlessly call
			// _update_input_source() a second time.
			emit_signal("select", p_input_source_id);
			break;

		case WEBXR_INPUT_EVENT_SQUEEZESTART:
			emit_signal("squeezestart", p_input_source_id);
			break;

		case WEBXR_INPUT_EVENT_SQUEEZEEND:
			emit_signal("squeezeend", p_input_source_id);
			// Again, we emit the 'squeeze' event on our own to avoid extra work.
			emit_signal("squeeze", p_input_source_id);
			break;
	}
}

Vector2 WebXRInterfaceJS::_get_screen_position_from_joy_vector(const Vector2 &p_joy_vector) {
	SceneTree *scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!scene_tree) {
		return Vector2();
	}

	Window *viewport = scene_tree->get_root();

	Vector2 position_percentage((p_joy_vector.x + 1.0f) / 2.0f, ((p_joy_vector.y) + 1.0f) / 2.0f);
	Vector2 position = (Size2)viewport->get_size() * position_percentage;

	return position;
}

WebXRInterfaceJS::WebXRInterfaceJS() {
	initialized = false;
	session_mode = "inline";
	requested_reference_space_types = "local";
};

WebXRInterfaceJS::~WebXRInterfaceJS() {
	// and make sure we cleanup if we haven't already
	if (initialized) {
		uninitialize();
	};
};

#endif // WEB_ENABLED