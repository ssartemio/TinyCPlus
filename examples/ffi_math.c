#include <stddef.h>
typedef struct CPoint { int x; double y; } CPoint;
enum CStatus { C_OK=0, C_INVALID=7 };
enum CStatus point_scale(CPoint *point,int scale) { if(!point) return C_INVALID; point->x*=scale; point->y*=scale; return C_OK; }
double point_sum(CPoint value) { return value.x+value.y; }
int apply_callback(int (*callback)(int),int value) { return callback(value); }
size_t point_size(void) { return sizeof(CPoint); }
