/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VECS_PER_SPECIFIC_BRUSH 1

#include shared,prim_shared,brush,yuv

flat varying vec3 vYuvLayers;

varying vec2 vUv_Y;
flat varying vec4 vUvBounds_Y;

varying vec2 vUv_U;
flat varying vec4 vUvBounds_U;

varying vec2 vUv_V;
flat varying vec4 vUvBounds_V;

flat varying float vCoefficient;
flat varying mat3 vYuvColorMatrix;
flat varying vec3 vYuvOffsetVector;
flat varying int vFormat;

#ifdef SWGL_DRAW_SPAN
flat varying int vYuvColorSpace;
flat varying int vRescaleFactor;
#endif

#ifdef WR_VERTEX_SHADER

struct YuvPrimitive {
    float coefficient;
    int color_space;
    int yuv_format;
};

YuvPrimitive fetch_yuv_primitive(int address) {
    vec4 data = fetch_from_gpu_cache_1(address);
    return YuvPrimitive(data.x, int(data.y), int(data.z));
}

void brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 unused
) {
    vec2 f = (vi.local_pos - local_rect.p0) / local_rect.size;

    YuvPrimitive prim = fetch_yuv_primitive(prim_address);
    vCoefficient = prim.coefficient;

    vYuvColorMatrix = get_yuv_color_matrix(prim.color_space);
    vYuvOffsetVector = get_yuv_offset_vector(prim.color_space);
    vFormat = prim.yuv_format;

#ifdef SWGL_DRAW_SPAN
    // swgl_commitTextureLinearYUV needs to know the color space specifier and
    // also needs to know how many bits of scaling are required to normalize
    // HDR textures.
    vYuvColorSpace = prim.color_space;
    vRescaleFactor = int(log2(prim.coefficient));
#endif

    if (vFormat == YUV_FORMAT_PLANAR) {
        ImageResource res_y = fetch_image_resource(prim_user_data.x);
        ImageResource res_u = fetch_image_resource(prim_user_data.y);
        ImageResource res_v = fetch_image_resource(prim_user_data.z);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE_YUV(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(res_u.uv_rect.p0, res_u.uv_rect.p1, f, TEX_SIZE_YUV(sColor1), vUv_U, vUvBounds_U);
        write_uv_rect(res_v.uv_rect.p0, res_v.uv_rect.p1, f, TEX_SIZE_YUV(sColor2), vUv_V, vUvBounds_V);
        vYuvLayers = vec3(res_y.layer, res_u.layer, res_v.layer);
    } else if (vFormat == YUV_FORMAT_NV12) {
        ImageResource res_y = fetch_image_resource(prim_user_data.x);
        ImageResource res_u = fetch_image_resource(prim_user_data.y);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE_YUV(sColor0), vUv_Y, vUvBounds_Y);
        write_uv_rect(res_u.uv_rect.p0, res_u.uv_rect.p1, f, TEX_SIZE_YUV(sColor1), vUv_U, vUvBounds_U);
        vYuvLayers = vec3(res_y.layer, res_u.layer, 0.0);
    } else if (vFormat == YUV_FORMAT_INTERLEAVED) {
        ImageResource res_y = fetch_image_resource(prim_user_data.x);
        write_uv_rect(res_y.uv_rect.p0, res_y.uv_rect.p1, f, TEX_SIZE_YUV(sColor0), vUv_Y, vUvBounds_Y);
        vYuvLayers = vec3(res_y.layer, 0.0, 0.0);
    }
}
#endif

#ifdef WR_FRAGMENT_SHADER

Fragment brush_fs() {
    vec4 color = sample_yuv(
        vFormat,
        vYuvColorMatrix,
        vYuvOffsetVector,
        vCoefficient,
        vYuvLayers,
        vUv_Y,
        vUv_U,
        vUv_V,
        vUvBounds_Y,
        vUvBounds_U,
        vUvBounds_V
    );

#ifdef WR_FEATURE_ALPHA_PASS
    color *= antialias_brush();
#endif

    return Fragment(color);
}

#ifdef SWGL_DRAW_SPAN
void swgl_drawSpanRGBA8() {
    if (vFormat == YUV_FORMAT_PLANAR) {
        if (!swgl_isTextureLinear(sColor0) || !swgl_isTextureLinear(sColor1) || !swgl_isTextureLinear(sColor2)) {
            return;
        }

        swgl_commitTextureLinearYUV(sColor0, vUv_Y, vUvBounds_Y, vYuvLayers.x,
                                    sColor1, vUv_U, vUvBounds_U, vYuvLayers.y,
                                    sColor2, vUv_V, vUvBounds_V, vYuvLayers.z,
                                    vYuvColorSpace, vRescaleFactor);
    } else if (vFormat == YUV_FORMAT_NV12) {
        if (!swgl_isTextureLinear(sColor0) || !swgl_isTextureLinear(sColor1)) {
            return;
        }

        swgl_commitTextureLinearYUV(sColor0, vUv_Y, vUvBounds_Y, vYuvLayers.x,
                                    sColor1, vUv_U, vUvBounds_U, vYuvLayers.y,
                                    vYuvColorSpace, vRescaleFactor);
    } else if (vFormat == YUV_FORMAT_INTERLEAVED) {
        if (!swgl_isTextureLinear(sColor0)) {
            return;
        }

        swgl_commitTextureLinearYUV(sColor0, vUv_Y, vUvBounds_Y, vYuvLayers.x,
                                    vYuvColorSpace, vRescaleFactor);
    }
}
#endif

#endif
