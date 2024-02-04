
void AMTS_SkeletonFromData(AMTS_Skeleton *skeleton, Str8 data) {
    AMTS_Header *header = cast(AMTS_Header *) data.data;

    if (header->magic == AMTS_MAGIC && header->version <= AMTS_VERSION) {
        skeleton->version   = header->version;
        skeleton->framerate = header->framerate;

        // @todo: we have the data length here, we should calculate what should be the file size based
        // on the parameters in the header and make sure we have enough data
        //

        skeleton->string_table.count = header->string_table_count;
        skeleton->string_table.data  = cast(U8 *) (header + 1);

        skeleton->num_bones     = header->num_bones;
        skeleton->num_tracks    = header->num_tracks;
        skeleton->total_samples = header->total_samples;

        skeleton->bones   = cast(AMTS_BoneInfo  *) (skeleton->string_table.data + skeleton->string_table.count);
        skeleton->tracks  = cast(AMTS_TrackInfo *) (skeleton->bones  + skeleton->num_bones);
        skeleton->samples = cast(AMTS_Sample    *) (skeleton->tracks + skeleton->num_tracks);
    }
}

void AMTS_SkeletonCopyFromData(Arena *arena, AMTS_Skeleton *skeleton, Str8 data) {
    AMTS_Header *header = cast(AMTS_Header *) data.data;

    if (header->magic == AMTS_MAGIC && header->version <= AMTS_VERSION) {
        // We have done our best and have detect this data is likely to be a skeleton file
        //
        skeleton->version   = header->version;
        skeleton->framerate = header->framerate;

        skeleton->string_table.count = header->string_table_count;

        skeleton->num_bones     = header->num_bones;
        skeleton->num_tracks    = header->num_tracks;
        skeleton->total_samples = header->total_samples;

        U8 *string_table        = cast(U8 *) (header + 1);
        AMTS_BoneInfo  *bones   = cast(AMTS_BoneInfo  *) (string_table + skeleton->string_table.count);
        AMTS_TrackInfo *tracks  = cast(AMTS_TrackInfo *) (bones  + skeleton->num_bones);
        AMTS_Sample    *samples = cast(AMTS_Sample    *) (tracks + skeleton->num_tracks);

        skeleton->string_table.data = ArenaPushCopy(arena, string_table, U8, skeleton->string_table.count);

        skeleton->bones   = ArenaPushCopy(arena, bones,   AMTS_BoneInfo,  skeleton->num_bones);
        skeleton->tracks  = ArenaPushCopy(arena, tracks,  AMTS_TrackInfo, skeleton->num_tracks);
        skeleton->samples = ArenaPushCopy(arena, samples, AMTS_Sample,    skeleton->total_samples);
    }
}
