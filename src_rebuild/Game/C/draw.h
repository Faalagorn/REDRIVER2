#ifndef DRAW_H
#define DRAW_H

extern SVECTOR day_vectors[4];
extern SVECTOR night_vectors[4];
extern SVECTOR day_colours[4];
extern SVECTOR night_colours[4];

extern MATRIX aspect;
extern MATRIX identity;
extern MATRIX inv_camera_matrix;
extern MATRIX face_camera;

extern MATRIX2 matrixtable[64];
extern MATRIX2 CompoundMatrix[64];

extern _pct& plotContext;

#ifdef PSX

#define MAX_DRAWN_BUILDINGS		192
#define MAX_DRAWN_TILES			320
#define MAX_DRAWN_ANIMATING		20
#define MAX_DRAWN_SPRITES		75

#define DRAW_LOD_DIST_HIGH		4000
#define DRAW_LOD_DIST_LOW		7000

#else

#define MAX_DRAWN_BUILDINGS		384
#define MAX_DRAWN_TILES			750
#define MAX_DRAWN_ANIMATING		48
#define MAX_DRAWN_SPRITES		128

#define DRAW_LOD_DIST_HIGH		7000
#define DRAW_LOD_DIST_LOW		10000

#endif

enum PlotFlags
{
	PLOT_TRANSPARENT = (1 << 0),
	PLOT_INV_CULL = (1 << 1),
	PLOT_NO_CULL = (1 << 2),
	PLOT_NO_SHADE = (1 << 3),
	PLOT_CUSTOM_PALETTE = (1 << 4),
};

extern void* model_tile_ptrs[MAX_DRAWN_TILES];

extern int units_across_halved;
extern int units_down_halved;
extern int pvs_square;
extern int pvs_square_sq;
extern int PolySizes[56];

extern int combointensity;

extern int gForceLowDetailCars;
extern int num_cars_drawn;

extern char CurrentPVS[444];

extern void DrawMapPSX(int *comp_val); // 0x0003F6B0

extern void SetupPlaneColours(u_int ambient); // 0x00040218
extern void SetupDrawMapPSX(); // 0x00040408

extern void InitFrustrumMatrix(); // 0x00040534
extern void SetFrustrumMatrix(); // 0x000417A4
extern void Set_Inv_CameraMatrix(); // 0x000417DC

extern void CalcObjectRotationMatrices(); // 0x00041814

extern void PlotMDL_less_than_128(MODEL *model); // 0x000418BC

extern void DrawAllTheCars(int view); // 0x000407D8

extern void RenderModel(MODEL *model, MATRIX *matrix, VECTOR *pos, int zBias, int flags, int subdiv, int nrot); // 0x0004143C


#endif
