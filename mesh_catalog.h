#pragma once

#include "common.h"

#include <glad/glad.h>
#include "catalog.h"

struct Texture_Map;

struct Triangle_List_Info
{
    i32 material_index = -1; // Which material to use to render these triangles? Index is inside 'material_info'.

    i32 num_indices = 0;    // How many vertices are there in the list? (must be a multiple of 3)
    i32 first_index = 0;    // What is the index of the beginning of this list? (like an offset)
};

struct Frame3
{
    Vector3 tangent;
    Vector3 bitangent;
    Vector3 normal;
};

struct Render_Material
{
    Vector4 albedo_color;
    Texture_Map *albedo_map = NULL;
};

struct Triangle_Mesh
{
    GLuint vertex_vbo = 0;
    GLuint index_vbo  = 0;

    // Imported vertex data:
    RArr<Vector3> vertices;
    RArr<Vector2> uvs;
    RArr<Frame3>  vertex_frames;
    RArr<Vector4> colors; // Only use colors if every triangle list uses colors.

    // Triangle data:
    RArr<i32> index_array;

    RArr<Triangle_List_Info> triangle_list_info;
    RArr<Render_Material>    material_info;

    // Catalog stuff.
    String name;
    String full_path;

    bool loaded = false; // For catalog
};

using Mesh_Catalog = Catalog<Triangle_Mesh>;

void init_mesh_catalog(Mesh_Catalog *catalog);
Triangle_Mesh *make_placeholder(Mesh_Catalog *catalog, String short_name, String full_name);
void reload_asset(Mesh_Catalog *catalog, Triangle_Mesh *mesh);
