#include "driver2.h"
#include "map.h"
#include "system.h"
#include "spool.h"
#include "convert.h"
#include "texture.h"
#include "mission.h"
#include "draw.h"
#include "camera.h"
#include "glaunch.h"
#include "models.h"
#include "players.h"
#include "main.h"

#include "STRINGS.H"

char *map_lump = NULL;
int initarea = 0;
int LoadedArea;

int current_region = 0;
int old_region = 0;

int current_cell_x = 0;
int current_cell_z = 0;

int num_regions;
int view_dist;
int pvs_square;
int pvs_square_sq;

int units_across_halved;
int units_down_halved;

int regions_across;
int regions_down;

PACKED_CELL_OBJECT** pcoplist;
CELL_OBJECT** coplist;

OUT_CELL_FILE_HEADER cell_header;

int cells_across = 1000;
int cells_down = 1000;

unsigned short* cell_ptrs;
PACKED_CELL_OBJECT* cell_objects;
int sizeof_cell_object_computed_values;
CELL_DATA* cells;
int num_straddlers;		// objects between regions

#define MAX_REGIONS 1024

// [D] [T]
void InitCellData(void)
{
	int loop;

	D_MALLOC_BEGIN();

	cell_ptrs = (ushort*)D_MALLOC(4096 * sizeof(short));

	// init cell pointers
	for (loop = 0; loop < 4096; loop++)
		cell_ptrs[loop] = 0xFF;

	// (mallocptr + 8192);
	cell_objects = (PACKED_CELL_OBJECT*)D_MALLOC((num_straddlers + cell_objects_add[4]) * sizeof(PACKED_CELL_OBJECT));
	cells = (CELL_DATA*)D_MALLOC(cell_slots_add[4] * sizeof(CELL_DATA));

	D_MALLOC_END();

	sizeof_cell_object_computed_values = (num_straddlers + cell_objects_add[4] + 7) / sizeof(PACKED_CELL_OBJECT);
}

// [D] [T]
void ProcessMapLump(char* lump_ptr, int lump_size)
{
	memcpy((u_char*)&cell_header, lump_ptr, sizeof(OUT_CELL_FILE_HEADER));

	cells_across = cell_header.cells_across;
	cells_down = cell_header.cells_down;
	num_regions = cell_header.num_regions;

#ifdef PSX
	if(num_regions > MAX_REGIONS)
	{
		printError("MAX_REGIONS is too small , allocate at least %d !", num_regions);
		trap(0x400);
	}

	if(cell_header.cell_size != MAP_CELL_SIZE)
	{
		printError("Error LevEdit cellsize %d whilst PSX cellsize %d\n", cell_header.cell_size, MAP_CELL_SIZE);
		trap(0x400);
	}

	if (cell_header.region_size != MAP_REGION_SIZE)
	{
		printError("Error LevEdit regionsize %d whilst PSX regionsize %d\n", cell_header.region_size, MAP_REGION_SIZE);
		trap(0x400);
	}
#endif

	view_dist = 10;
	pvs_square = 21;
	pvs_square_sq = 21 * 21;

	units_across_halved = cells_across / 2 * MAP_CELL_SIZE;
	units_down_halved = cells_down / 2 * MAP_CELL_SIZE;

	regions_across = cells_across / MAP_REGION_SIZE;
	regions_down = cells_down / MAP_REGION_SIZE;

	lump_ptr += sizeof(OUT_CELL_FILE_HEADER);

	num_straddlers = *(int*)lump_ptr;

	InitCellData();
	memcpy((u_char*)cell_objects, lump_ptr + 4, num_straddlers * sizeof(PACKED_CELL_OBJECT));
}


// [D] [T]
void NewProcessRoadMapLump(ROAD_MAP_LUMP_DATA *pRoadMapLumpData, char *pLumpFile)
{
	Getlong((char*)&pRoadMapLumpData->width, pLumpFile);
	Getlong((char*)&pRoadMapLumpData->height, pLumpFile + 4);

	pRoadMapLumpData->unitXMid = (pRoadMapLumpData->width + 1) * 512;
	pRoadMapLumpData->unitZMid = pRoadMapLumpData->height * 512;
}

// [D] [T]
void ProcessJunctionsLump(char *lump_file, int lump_size)
{
	return;
}


// [D] [T]
void ProcessRoadsLump(char *lump_file, int lump_size)
{
	return;
}

// [D] [T]
void ProcessRoadBoundsLump(char *lump_file, int lump_size)
{
	return;
}

// [D] [T]
void ProcessJuncBoundsLump(char *lump_file, int lump_size)
{
	return;
}

// [D] [T]
int newPositionVisible(VECTOR *pos, char *pvs, int ccx, int ccz)
{
 	int dx; // $a2
 	int dz; // $a0
 	int cellx; // $v1
 	int cellz; // $v0

	dx = pos->vx + units_across_halved;
	dz = pos->vz + units_down_halved;

	cellx = (dx / MAP_CELL_SIZE) - ccx;
	cellz = (dz / MAP_CELL_SIZE) - ccz;

	if (ABS(cellx) <= view_dist && ABS(cellz) <= view_dist)
	{
		return pvs[cellx + 10 + (cellz + 10) * pvs_square] != 0;
	}

	return 0;
}

// [D] [T]
int PositionVisible(VECTOR *pos)
{
 	int dx; // $a1
 	int dz; // $a0
 	int cellx; // $v1
 	int cellz; // $v0

	int ab;

	dx = pos->vx + units_across_halved;
	dz = pos->vz + units_down_halved;

	cellx = (dx / MAP_CELL_SIZE) - current_cell_x;
	cellz = (dz / MAP_CELL_SIZE) - current_cell_z;

	if (cellx < 0)
		ab = -cellx;
	else
		ab = cellx;

	if (ab <= view_dist)
	{
		if (cellz < 0)
			ab = -cellz;
		else
			ab = cellz;

		if (ab <= view_dist)
			return CurrentPVS[cellx + 10 + (cellz + 10) * pvs_square] != 0;
	}

	return 0;
}


// FIXME: move it somewhere else
extern int saved_leadcar_pos;

int region_x = 0;
int region_z = 0;

int current_barrel_region_xcell = 0;
int current_barrel_region_zcell = 0;

// [D] [T]
int CheckUnpackNewRegions(void)
{
	ushort sort;
	ushort *destsort;
	ushort *srcsort;

	int x, z;
	int i, j;
	int sortcount;
	int leftright_unpack;
	int topbottom_unpack;
	int target_region;
	int region_to_unpack;
	int num_regions_to_unpack;
	int force_load_boundary;
	AREA_LOAD_INFO regions_to_unpack[3];
	SVECTOR sortregions[4];
	ushort sortorder[4];

	leftright_unpack = 0;
	topbottom_unpack = 0;
	num_regions_to_unpack = 0;

	if (saved_leadcar_pos != 0) 
		return 0;

	force_load_boundary = 13;

	if (lead_car == 0)
		force_load_boundary = 18;

	if (current_barrel_region_xcell < force_load_boundary)
	{
		if (region_x != 0) 
		{
			leftright_unpack = 1;
			num_regions_to_unpack = 1;
			regions_to_unpack[0].xoffset = -1;
			regions_to_unpack[0].zoffset = 0;
		}
	}
	else if (32 - force_load_boundary < current_barrel_region_xcell)
	{
		if (region_x < cells_across >> 5)
		{
			leftright_unpack = 2;
			num_regions_to_unpack = 1;
			regions_to_unpack[0].xoffset = 1;
			regions_to_unpack[0].zoffset = 0;
		}
	}

	if (current_barrel_region_zcell < force_load_boundary)
	{
		if (region_z != 0) 
		{
			topbottom_unpack = 1;
			regions_to_unpack[num_regions_to_unpack].xoffset = 0;
			regions_to_unpack[num_regions_to_unpack].zoffset = -1;
			num_regions_to_unpack++;
		}
	}
	else if (32 - force_load_boundary < current_barrel_region_zcell && region_z != 0)
	{
		topbottom_unpack = 2;
		regions_to_unpack[num_regions_to_unpack].xoffset = 0;
		regions_to_unpack[num_regions_to_unpack].zoffset = 1;
		num_regions_to_unpack++;
	}

	if (num_regions_to_unpack == 2)
	{
		if (topbottom_unpack == 1)
		{
			num_regions_to_unpack = 3;

			if (leftright_unpack == 1)
			{
				regions_to_unpack[2].xoffset = -1;
				regions_to_unpack[2].zoffset = -1;
			}
			else
			{
				regions_to_unpack[2].xoffset = 1;
				regions_to_unpack[2].zoffset = -1;
			}
		}
		else 
		{
			if (leftright_unpack == 1)
				regions_to_unpack[2].xoffset = -1;
			else 
				regions_to_unpack[2].xoffset = 1;

			regions_to_unpack[2].zoffset = 1;
		}
		num_regions_to_unpack = 3;
	}

	i = 0;
	sortcount = 0;

	// get next region a space
	while (i < num_regions_to_unpack)
	{
		x = regions_to_unpack[i].xoffset;
		z = regions_to_unpack[i].zoffset;

		target_region = (region_x + x & 1U) + (region_z + z & 1U) * 2;
		region_to_unpack = current_region + x + z * (cells_across / 32);

		if (region_to_unpack != regions_unpacked[target_region] && loading_region[target_region] == -1)
		{
			ClearRegion(target_region);

			if (spoolinfo_offsets[region_to_unpack] == 0xffff)
			{
				regions_unpacked[target_region] = region_to_unpack;
			}
			else 
			{
				Spool *spoolptr = (Spool*)(RegionSpoolInfo + spoolinfo_offsets[region_to_unpack]);

				if (old_region == -1 && spoolptr->super_region != 0xFF)
					initarea = spoolptr->super_region;

				sortregions[sortcount].vx = region_to_unpack;
				sortregions[sortcount].vy = target_region;
				sortregions[sortcount].vz = spoolptr->offset;

				sortorder[sortcount] = sortcount;
				sortcount++;
			}
		}
		i++;
	}

	i = 0;
	while (i < sortcount)
	{
		if (sortcount > (i + 1))
		{
			srcsort = sortorder + i;
			destsort = sortorder + (i + 1);

			j = sortcount - (i + 1);

			do {
				sort = *srcsort;
				if (sortregions[*destsort].vz < sortregions[*srcsort].vz)
				{
					*srcsort = *destsort;
					*destsort = sort;
				}

				j--;
				destsort++;
			} while (j > 0);
		}

		UnpackRegion(sortregions[sortorder[i]].vx, sortregions[sortorder[i]].vy);

		i++;
	}

	return 1;
}

// [D] [T]
void ControlMap(void)
{
	int region_to_unpack;

	current_cell_x = (player[0].spoolXZ->vx + units_across_halved) / MAP_CELL_SIZE;
	current_cell_z = (player[0].spoolXZ->vz + units_down_halved) / MAP_CELL_SIZE;

	region_x = current_cell_x / MAP_REGION_SIZE;
	region_z = current_cell_z / MAP_REGION_SIZE;

	old_region = current_region;

	current_barrel_region_xcell = current_cell_x - region_x * MAP_REGION_SIZE;
	current_barrel_region_zcell = current_cell_z - region_z * MAP_REGION_SIZE;

	region_to_unpack = region_x + region_z * regions_across;

	if (current_region == -1)
		UnpackRegion(region_to_unpack, region_x & 1U | (region_z & 1U) * 2);		// is that ever valid for 'target_barrel_region'?

	current_region = region_to_unpack;
	
	CheckLoadAreaData(current_barrel_region_xcell, current_barrel_region_zcell);

	CheckUnpackNewRegions();

	current_cell_x = (camera_position.vx + units_across_halved) / MAP_CELL_SIZE;
	current_cell_z = (camera_position.vz + units_down_halved) / MAP_CELL_SIZE;

	StartSpooling();
}

// [D] [T]
void InitMap(void)
{
	int region_to_unpack;
	int barrel_region;
	int i;

	initarea = -1;
	LoadedArea = -1;
	current_region = -1;

	i = slotsused;

	while (i < 19)
	{
		int tpage = tpageslots[i];

		if (tpage != 0xff)
			tpageloaded[tpage] = 0;

		tpageslots[i] = 0xff;
		i++;
	}

	// load regions synchronously
	if (doSpooling == 0)
	{
		old_region = -1;

		if (multiplayerregions[1] == -1)
		{
			multiplayerregions[1] = multiplayerregions[0] + 1;
			multiplayerregions[2] = multiplayerregions[0] - (cells_across >> 5);
			multiplayerregions[3] = multiplayerregions[2] + 1;
		}

		i = 0;
		do {
			region_to_unpack = multiplayerregions[i];

			if (spoolinfo_offsets[region_to_unpack] != 0xffff)
			{
				Spool* spoolptr = (Spool*)(RegionSpoolInfo + spoolinfo_offsets[region_to_unpack]);

				if (spoolptr->super_region != 0xff)
					initarea = spoolptr->super_region;

				barrel_region = (region_to_unpack % (cells_across >> 5) & 1U) + (region_to_unpack / (cells_across >> 5) & 1U) * 2;

				UnpackRegion(region_to_unpack, barrel_region);
			}

			i++;
		} while (i < 4);

		LoadInAreaTSets(initarea);
		LoadInAreaModels(initarea);

		current_cell_x = (camera_position.vx + units_across_halved) / MAP_CELL_SIZE;
		current_cell_z = (camera_position.vz + units_down_halved) / MAP_CELL_SIZE;

		StartSpooling();
	}
	else
	{
		regions_unpacked[0] = -1;
		regions_unpacked[1] = -1;
		regions_unpacked[2] = -1;
		regions_unpacked[3] = -1;

		ControlMap();
	}
}

// [D] [T] [A]
void GetVisSetAtPosition(VECTOR *pos, char *tgt, int *ccx, int *ccz)
{
	int cz;
	int cx;

	int rz;
	int rx;

	cx = (pos->vx + units_across_halved) / MAP_CELL_SIZE;
	cz = (pos->vz + units_down_halved) / MAP_CELL_SIZE;

	*ccx = cx;
	*ccz = cz;

	rx = cx / MAP_REGION_SIZE;
	rz = cz / MAP_REGION_SIZE;

	int barrel_region_x = (rx & 1);
	int barrel_region_z = (rz & 1);

	GetPVSRegionCell2(
		barrel_region_x + barrel_region_z * 2,
		rx + rz * regions_across,
		(cz - rz * MAP_REGION_SIZE) * MAP_REGION_SIZE + cx - rx * MAP_REGION_SIZE,
		tgt);
}

unsigned char nybblearray[512] = { 0 };

char* PVS_Buffers[4] = { 0 };
int pvsSize[4] = { 0, 0, 0, 0 };
unsigned char *PVSEncodeTable = NULL;

// [D] [T]
void PVSDecode(char *output, char *celldata, ushort sz, int havanaCorruptCellBodge)
{
	unsigned char scratchPad[1024];

	int pixelIndex;
	unsigned char* decodebuf;
	unsigned char* op;
	int i, j;
	int symIndex;
	int size;

	decodebuf = scratchPad;
	ClearMem((char*)decodebuf,pvs_square_sq);

	// decode byte-swapped array
	i = 0;
	while (i < sz)
	{
		((ushort*)nybblearray)[i] = M_SHRT_2((unsigned char)celldata[i], (unsigned char)celldata[i] >> 4) & 0xf0f;
		i++;
	}

	pixelIndex = 0;
	symIndex = 0;
	i = 0;

	// decompress image
	while (i < sz * 2)
	{
		int ni;
		int sym;

		ni = nybblearray[i];
		i++;

		if (ni < 12)
		{
			symIndex = ni * 2;
		spod:
			sym = M_SHRT_2(PVSEncodeTable[symIndex], PVSEncodeTable[symIndex + 1]);
		}
		else
		{
			if (i == sz*2)
				break;

			sym = (ni & 3) * 16 + nybblearray[i];
			i++;

			if (sym < 60)
			{
				symIndex = sym * 2 + 24;
				goto spod;
			}

			sym = ((sym & 3) * 16 + nybblearray[i]) * 16 + nybblearray[i+1];
			i += 2;
		}

		pixelIndex += (sym >> 1);
		decodebuf[pixelIndex] = 1;
		pixelIndex++;

		if ((sym & 1) != 0)
		{
			decodebuf[pixelIndex] = 1;
			pixelIndex++;
		}
	}

	if (havanaCorruptCellBodge == 0) 
		decodebuf[pvs_square_sq-1] ^= 1;

	size = pvs_square - 2;
	
	op = (decodebuf - 1) + (size * pvs_square + pvs_square);
	i = size;
	while (i >= 0) 
	{
		i--;
		j = pvs_square;
		while (j > 0)
		{
			*op = *op ^ op[pvs_square];
			j--;
			op--;
		}
	}

	size = pvs_square - 1;
	op = (decodebuf - 2) + (size * pvs_square + pvs_square);
	i = size;

	while (i >= 0) 
	{
		j = pvs_square-2;
		while (j >= 0) 
		{
			*op = *op ^ op[1];
			j--;
			op--;
		}
		i--;
		op--;
	}

#if 0
	printf("=========================\n");
	for (i = 0; i < pvs_square; i++)
	{
		char line[64];
		memcpy(line, (char*)decodebuf + i * pvs_square, pvs_square);
		for (j = 0; j < pvs_square; j++)
		{
			if (line[j] == 0)
				line[j] = '.';
			else
				line[j] = 'O';
		}
		line[j] = 0;
		printf("%s\n", line);
	}
	printf("=========================\n");
#endif
	memcpy((u_char*)output, decodebuf, pvs_square_sq-1);	// 110*4
}


// [D] [T]
void GetPVSRegionCell2(int source_region, int region, int cell, char *output)
{
	int k;
	u_int havanaCorruptCellBodge;
	char *tbp;
	char *bp;
	ushort length;

#ifndef PSX
	if (gDemoLevel)
	{
		// don't draw non-loaded regions
		for (k = 0; k < pvs_square_sq; k++)
			output[k] = 1;

		return;
	}
#endif

	if (regions_unpacked[source_region] == region && loading_region[source_region] == -1) 
	{
		bp = PVS_Buffers[source_region];
		PVSEncodeTable = (unsigned char *)(bp + 0x802);
		tbp = bp + cell * 2;

		length = M_SHRT_2((unsigned char)tbp[2], (unsigned char)tbp[3]) - M_SHRT_2((unsigned char)tbp[0], (unsigned char)tbp[1]) & 0xffff;

		if (length == 0) 
		{
			for (k = 0; k < pvs_square_sq; k++)
				output[k] = 1;
		}
		else 
		{
			havanaCorruptCellBodge = 0;
			if (regions_unpacked[source_region] == 158 && cell == 168) 
				havanaCorruptCellBodge = (GameLevel == 1);

			PVSDecode(output, bp + M_SHRT_2((unsigned char)tbp[0], (unsigned char)tbp[1]), length, havanaCorruptCellBodge);
		}
	}
	else 
	{
		// don't draw non-loaded regions
		for (k = 0; k < pvs_square_sq; k++)
			output[k] = 0;
	}
}





