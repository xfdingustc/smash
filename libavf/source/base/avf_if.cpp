
#include "avf_common.h"
#include "avf_osal.h"
#include "avf_if.h"
#include "avf_base_if.h"

AVF_DEFINE_GUID(GUID_NIL,
0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

//{E91F0CD5-D7D9-4BED-B056-05A3C69861E3}
AVF_DEFINE_IID(IID_IAllocMem,
0xE91F0CD5, 0xD7D9, 0x4BED, 0xB0, 0x56, 0x5, 0xA3, 0xC6, 0x98, 0x61, 0xE3);

//{1C6C840D-A239-4F23-9366-2B10F5BD3ADB}
AVF_DEFINE_IID(IID_IInterface,
0x1C6C840D, 0xA239, 0x4F23, 0x93, 0x66, 0x2B, 0x10, 0xF5, 0xBD, 0x3A, 0xDB);

//{DE6BB5F2-E87A-434A-BB3F-655D8F2AE9AC}
AVF_DEFINE_IID(IID_IRegistry,
0xDE6BB5F2, 0xE87A, 0x434A, 0xBB, 0x3F, 0x65, 0x5D, 0x8F, 0x2A, 0xE9, 0xAC);

//{5A63E476-F981-4F4B-97ED-F60AF529B6AD}
AVF_DEFINE_IID(IID_IMediaControl,
0x5A63E476, 0xF981, 0x4F4B, 0x97, 0xED, 0xF6, 0xA, 0xF5, 0x29, 0xB6, 0xAD);

//{E78498EF-2240-4248-92B4-90AA6672F74A}
AVF_DEFINE_IID(IID_IVDBControl,
0xE78498EF, 0x2240, 0x4248, 0x92, 0xB4, 0x90, 0xAA, 0x66, 0x72, 0xF7, 0x4A);

//{2DC3E24E-6744-4A8C-9536-5CE9F227CD38}
AVF_DEFINE_IID(IID_IUploadManager,
0x2DC3E24E, 0x6744, 0x4A8C, 0x95, 0x36, 0x5C, 0xE9, 0xF2, 0x27, 0xCD, 0x38);

//{BCB17182-50BD-4D08-8C3D-484C29E12448}
AVF_DEFINE_IID(IID_IStorageListener,
0xBCB17182, 0x50BD, 0x4D08, 0x8C, 0x3D, 0x48, 0x4C, 0x29, 0xE1, 0x24, 0x48);

//{D4DA65DE-03AF-4584-92DA-1D46F96A6E97}
AVF_DEFINE_IID(IID_IActiveObject,
0xD4DA65DE, 0x3AF, 0x4584, 0x92, 0xDA, 0x1D, 0x46, 0xF9, 0x6A, 0x6E, 0x97);

//{2D6653DB-F691-4B78-8CE7-ECED1212F5C8}
AVF_DEFINE_IID(IID_IBufferPool,
0x2D6653DB, 0xF691, 0x4B78, 0x8C, 0xE7, 0xEC, 0xED, 0x12, 0x12, 0xF5, 0xC8);

//{44851C2D-B08D-49A7-BBB6-9513F3DBD63B}
AVF_DEFINE_IID(IID_IEngine,
0x44851C2D, 0xB08D, 0x49A7, 0xBB, 0xB6, 0x95, 0x13, 0xF3, 0xDB, 0xD6, 0x3B);

//{613F6ED5-FA3E-46A1-AE46-C63FC26A1C04}
AVF_DEFINE_IID(IID_ITimerManager,
0x613F6ED5, 0xFA3E, 0x46A1, 0xAE, 0x46, 0xC6, 0x3F, 0xC2, 0x6A, 0x1C, 0x4);

//{C3F7A7A6-910A-4072-8BCC-090AA1244F91}
AVF_DEFINE_IID(IID_ITimerTarget,
0xC3F7A7A6, 0x910A, 0x4072, 0x8B, 0xCC, 0x9, 0xA, 0xA1, 0x24, 0x4F, 0x91);

//{55CF0CF8-57C1-4682-AEAE-90BD5557B9C7}
AVF_DEFINE_IID(IID_ITimerReceiver,
0x55CF0CF8, 0x57C1, 0x4682, 0xAE, 0xAE, 0x90, 0xBD, 0x55, 0x57, 0xB9, 0xC7);

//0xC3A73886, 0xBE1B, 0x444D, 0x9C, 0x8, 0xC4, 0x3F, 0x60, 0xE7, 0x69, 0x56
AVF_DEFINE_IID(IID_IMediaPlayer,
0xC3A73886, 0xBE1B, 0x444D, 0x9C, 0x8, 0xC4, 0x3F, 0x60, 0xE7, 0x69, 0x56);

//{8A462BE9-85C7-4726-945F-5F88B105BF50}
AVF_DEFINE_IID(IID_IPlayback,
0x8A462BE9, 0x85C7, 0x4726, 0x94, 0x5F, 0x5F, 0x88, 0xB1, 0x5, 0xBF, 0x50);

//{CB9A1867-89B7-4897-9AED-2C960176001E}
AVF_DEFINE_IID(IID_IFilter,
0xCB9A1867, 0x89B7, 0x4897, 0x9A, 0xED, 0x2C, 0x96, 0x1, 0x76, 0x0, 0x1E);

//{639CE48C-4019-402D-BF04-E97BD50B92BD}
AVF_DEFINE_IID(IID_IPin,
0x639CE48C, 0x4019, 0x402D, 0xBF, 0x4, 0xE9, 0x7B, 0xD5, 0xB, 0x92, 0xBD);

//{A9B5D446-904A-4867-8783-D82EC80EE8CD}
AVF_DEFINE_GUID(IID_IMediaRenderer,
0xA9B5D446, 0x904A, 0x4867, 0x87, 0x83, 0xD8, 0x2E, 0xC8, 0xE, 0xE8, 0xCD);

//{52DA3BB2-EC65-4DBE-B4EC-27DE4E77083F}
AVF_DEFINE_IID(IID_IAVIO,
0x52DA3BB2, 0xEC65, 0x4DBE, 0xB4, 0xEC, 0x27, 0xDE, 0x4E, 0x77, 0x8, 0x3F);

// {377B3B59-920F-4A2F-BCAB-95B5F2C316BF}
AVF_DEFINE_IID(IID_IBlockIO,
0x377B3B59, 0x920F, 0x4A2F, 0xBC, 0xAB, 0x95, 0xB5, 0xF2, 0xC3, 0x16, 0xBF);

// {F3547C84-0DB2-475D-9B2B-8E6557226ABA}
AVF_DEFINE_IID(IID_ITSMapInfo,
0xF3547C84, 0xDB2, 0x475D, 0x9B, 0x2B, 0x8E, 0x65, 0x57, 0x22, 0x6A, 0xBA);

//{6BEF1C06-D187-4436-B619-952AAB36DCCF}
AVF_DEFINE_IID(IID_IMediaFileReader,
0x6BEF1C06, 0xD187, 0x4436, 0xB6, 0x19, 0x95, 0x2A, 0xAB, 0x36, 0xDC, 0xCF);

//{35C02860-1979-4B62-A024-F94338677F9A}
AVF_DEFINE_IID(IID_IReadProgress,
0x35C02860, 0x1979, 0x4B62, 0xA0, 0x24, 0xF9, 0x43, 0x38, 0x67, 0x7F, 0x9A);

//{D7B39074-026F-4ACF-A243-98F6F81C2F7D}
AVF_DEFINE_IID(IID_IMediaTrack,
0xD7B39074, 0x26F, 0x4ACF, 0xA2, 0x43, 0x98, 0xF6, 0xF8, 0x1C, 0x2F, 0x7D);

//{DF9A2D28-1311-4FBE-843E-9B6605FE90B2}
AVF_DEFINE_IID(IID_IRecordObserver,
0xDF9A2D28, 0x1311, 0x4FBE, 0x84, 0x3E, 0x9B, 0x66, 0x5, 0xFE, 0x90, 0xB2);

//{61F2F212-FDEC-4170-994C-9A712B8B70CC}
AVF_DEFINE_IID(IID_IVdbInfo,
0x61F2F212, 0xFDEC, 0x4170, 0x99, 0x4C, 0x9A, 0x71, 0x2B, 0x8B, 0x70, 0xCC);

// {2E425B3F-10DB-4450-B874-9594E2F2308B}
AVF_DEFINE_IID(IID_IClipReader,
0x2E425B3F, 0x10DB, 0x4450, 0xB8, 0x74, 0x95, 0x94, 0xE2, 0xF2, 0x30, 0x8B);

// {2E65EE7D-4901-412B-BDF4-C9558E55AA4F}
AVF_DEFINE_IID(IID_IClipWriter,
0x2E65EE7D, 0x4901, 0x412B, 0xBD, 0xF4, 0xC9, 0x55, 0x8E, 0x55, 0xAA, 0x4F);

// {C3EDA697-73E8-47BD-A35A-EBCBAF233A8B}
AVF_DEFINE_IID(IID_ISeekToTime,
0xC3EDA697, 0x73E8, 0x47BD, 0xA3, 0x5A, 0xEB, 0xCB, 0xAF, 0x23, 0x3A, 0x8B);

// {941DCE0A-E758-40EE-B6D4-42168D12710D}
AVF_DEFINE_IID(IID_IClipData,
0x941DCE0A, 0xE758, 0x40EE, 0xB6, 0xD4, 0x42, 0x16, 0x8D, 0x12, 0x71, 0xD);

//{7491C0FC-B2A8-41C3-9032-C3C6E0B73D71}
AVF_DEFINE_IID(IID_ISegmentSaver,
0x7491C0FC, 0xB2A8, 0x41C3, 0x90, 0x32, 0xC3, 0xC6, 0xE0, 0xB7, 0x3D, 0x71);

//{46864EF6-D157-47E0-9EDC-EBFDC2909F31}
AVF_DEFINE_IID(IID_IMediaFileWriter,
0x46864EF6, 0xD157, 0x47E0, 0x9E, 0xDC, 0xEB, 0xFD, 0xC2, 0x90, 0x9F, 0x31);

//{4FF53EC8-AA98-46AE-AF94-B0CF07C29DCE}
AVF_DEFINE_IID(IID_ISampleProvider,
0x4FF53EC8, 0xAA98, 0x46AE, 0xAF, 0x94, 0xB0, 0xCF, 0x7, 0xC2, 0x9D, 0xCE);


/*
{941DCE0A-E758-40EE-B6D4-42168D12710D}
0x941DCE0A, 0xE758, 0x40EE, 0xB6, 0xD4, 0x42, 0x16, 0x8D, 0x12, 0x71, 0xD
{E1F561E0-4168-4341-8ABF-A7E9237AB8A6}
0xE1F561E0, 0x4168, 0x4341, 0x8A, 0xBF, 0xA7, 0xE9, 0x23, 0x7A, 0xB8, 0xA6
*/

