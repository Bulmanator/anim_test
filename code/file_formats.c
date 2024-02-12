
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

void AMTM_MeshFromData(Arena *arena, AMTM_Mesh *mesh, Str8 data) {
    AMTM_Header *header = cast(AMTM_Header *) data.data;
    if (header->magic == AMTM_MAGIC && header->version <= AMTM_VERSION) {
        // Found something that looks like what we want
        //
        mesh->version = header->version;

        mesh->num_materials = header->num_materials;
        mesh->num_textures  = header->num_textures;
        mesh->num_submeshes = header->num_meshes;

        mesh->submeshes = ArenaPush(arena, AMTM_Submesh, mesh->num_submeshes);

        Str8 string_table;
        string_table.count = header->string_table_count;
        string_table.data  = cast(U8 *) (header + 1);

        mesh->string_table = string_table;

        mesh->materials = cast(AMTM_Material *) (string_table.data + string_table.count);
        mesh->textures  = cast(AMTM_Texture  *) (mesh->materials + mesh->num_materials);

        AMTM_MeshInfo *info = cast(AMTM_MeshInfo *) (mesh->textures + mesh->num_textures);

        for (U32 it = 0; it < mesh->num_submeshes; ++it) {
            AMTM_Submesh *submesh = &mesh->submeshes[it];

            B32 is_skinned  = (info->flags & AMTM_MESH_FLAG_IS_SKINNED) != 0;
            U64 vertex_size = is_skinned ? sizeof(AMTM_SkinnedVertex) : sizeof(AMTM_Vertex);

            submesh->info     = info;
            submesh->vertices = cast(AMTM_Vertex *) (info + 1);
            submesh->indices  = cast(U16 *) ((U8 *) submesh->vertices + (info->num_vertices * vertex_size));

            info = cast(AMTM_MeshInfo *) (submesh->indices + info->num_indices);
        }
    }
}

void AMTM_MeshCopyFromData(Arena *arena, AMTM_Mesh *mesh, Str8 data) {
    AMTM_Header *header = cast(AMTM_Header *) data.data;
    if (header->magic == AMTM_MAGIC && header->version <= AMTM_VERSION) {
        mesh->version = header->version;

        mesh->num_materials = header->num_materials;
        mesh->num_textures  = header->num_textures;
        mesh->num_submeshes = header->num_meshes;

        mesh->submeshes = ArenaPush(arena, AMTM_Submesh, mesh->num_submeshes);

        Str8 string_table;
        string_table.count = header->string_table_count;
        string_table.data  = cast(U8 *) (header + 1);

        mesh->string_table.count = string_table.count;
        mesh->string_table.data  = ArenaPushCopy(arena, string_table.data, U8, mesh->string_table.count);

        AMTM_Material *materials = cast(AMTM_Material *) (string_table.data + string_table.count);
        AMTM_Texture  *textures  = cast(AMTM_Texture  *) (materials + mesh->num_materials);

        mesh->materials = ArenaPushCopy(arena, materials, AMTM_Material, mesh->num_materials);
        mesh->textures  = ArenaPushCopy(arena, textures,  AMTM_Texture,  mesh->num_textures);

        AMTM_MeshInfo *info = cast(AMTM_MeshInfo *) (textures + mesh->num_textures);

        for (U32 it = 0; it < mesh->num_submeshes; ++it) {
            AMTM_Submesh *submesh = &mesh->submeshes[it];

            submesh->info = ArenaPushCopy(arena, info, AMTM_MeshInfo);

            U16 *indices;
            B32 is_skinned = (info->flags & AMTM_MESH_FLAG_IS_SKINNED) != 0;

            if (is_skinned) {
                AMTM_SkinnedVertex *vertices = cast(AMTM_SkinnedVertex *) (info + 1);
                submesh->skinned_vertices = ArenaPushCopy(arena, vertices, AMTM_SkinnedVertex, info->num_vertices);

                indices = cast(U16 *) (vertices + info->num_vertices);
            }
            else {
                AMTM_Vertex *vertices = cast(AMTM_Vertex *) (info + 1);
                submesh->vertices = ArenaPushCopy(arena, vertices, AMTM_Vertex, info->num_vertices);

                indices = cast(U16 *) (vertices + info->num_vertices);
            }

            submesh->indices = ArenaPushCopy(arena, indices, U16, info->num_indices);

            info = cast(AMTM_MeshInfo *) (indices + info->num_indices);
        }
    }
}
