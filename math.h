#ifndef MATH_H
#define MATH_H

#include <math.h>

struct vector {
    float x, y, z;
};

struct matrix_4x4 {
    float m[4][4];
};

static inline float vector_dot(const struct vector* a, const struct vector* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static inline float vector_length(const struct vector* v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static inline struct vector vector_sub(const struct vector* a, const struct vector* b) {
    struct vector out;
    out.x = a->x - b->x;
    out.y = a->y - b->y;
    out.z = a->z - b->z;
    return out;
}

#ifndef RAD2DEG
#define RAD2DEG(x) ((x) * (180.0f / 3.14159265358979323846f))
#endif

static inline int world_to_screen(
    const struct vector* origin,
    const struct matrix_4x4* matrix,
    struct vector* out,
    const struct vector* screen) {

    float clip_x =
        matrix->m[0][0] * origin->x +
        matrix->m[0][1] * origin->y +
        matrix->m[0][2] * origin->z +
        matrix->m[0][3];

    float clip_y =
        matrix->m[1][0] * origin->x +
        matrix->m[1][1] * origin->y +
        matrix->m[1][2] * origin->z +
        matrix->m[1][3];

    float clip_w =
        matrix->m[3][0] * origin->x +
        matrix->m[3][1] * origin->y +
        matrix->m[3][2] * origin->z +
        matrix->m[3][3];

    if (clip_w < 0.001f)
        return 0;

    float inv_w = 1.0f / clip_w;
    float ndc_x = clip_x * inv_w;
    float ndc_y = clip_y * inv_w;

    out->x = (screen->x / 2.0f) * (1.0f + ndc_x);
    out->y = (screen->y / 2.0f) * (1.0f - ndc_y);
    out->z = 0.0f;

    if (out->x < 0.0f || out->x > screen->x || out->y < 0.0f || out->y > screen->y)
        return 0;

    return 1;
}

static inline float normalize_angle(float angle) {
    if (!isfinite(angle))
        return 0.0f;

    angle = fmodf(angle, 360.0f);
    if (angle > 180.0f)
        angle -= 360.0f;
    if (angle < -180.0f)
        angle += 360.0f;

    return angle;
}

static inline void clamp_angle(struct vector* angles) {
    angles->x = normalize_angle(angles->x);
    angles->y = normalize_angle(angles->y);
    angles->z = 0.0f;

    if (angles->x > 89.0f)
        angles->x = 89.0f;
    if (angles->x < -89.0f)
        angles->x = -89.0f;
}

static inline struct vector calc_angle(
    const struct vector* src,
    const struct vector* dst,
    const struct vector* view_angles) {
    struct vector delta = vector_sub(dst, src);

    float hyp = sqrtf(
        delta.x * delta.x +
        delta.y * delta.y);

    struct vector angles;
    angles.x = RAD2DEG(atan2f(-delta.z, hyp)) - view_angles->x;
    angles.y = RAD2DEG(atan2f(delta.y, delta.x)) - view_angles->y;
    angles.z = 0.0f;

    clamp_angle(&angles);

    return angles;
}

#endif