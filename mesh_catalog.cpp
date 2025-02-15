#include "mesh_catalog.h"
#include "file_utils.h"
#include "opengl.h"
#include "texture.h"

#include <fast_obj.h>

void init_mesh_catalog(Mesh_Catalog *catalog)
{
    catalog->base.my_name = String("Triangle Mesh");
    array_add(&catalog->base.extensions, String("obj"));
    do_polymorphic_catalog_init(catalog);
}

Triangle_Mesh *make_placeholder(Mesh_Catalog *catalog, String short_name, String full_name)
{
    auto mesh       = New<Triangle_Mesh>();
    mesh->name      = copy_string(short_name);
    mesh->full_path = copy_string(full_name);

    return mesh;
}

static void free_resources(Triangle_Mesh *mesh)
{
    array_free(&mesh->vertices);
    array_free(&mesh->uvs);
    array_free(&mesh->vertex_frames);
    array_free(&mesh->colors);

    array_free(&mesh->index_array);

    array_free(&mesh->triangle_list_info);
    array_free(&mesh->material_info);
}

inline
static void make_vertex_buffer(Triangle_Mesh *mesh)
{
    auto count       = mesh->vertices.count;
    auto dest_buffer = NewArray<Vertex_XCNUU>(count);

    for (i64 it_index = 0; it_index < count; ++it_index)
    {
        auto dest = &dest_buffer[it_index];

        // Every mesh supposed to have a vertex, but what the heyy, consistency!!?!?!?
        if (mesh->vertices) dest->position = mesh->vertices[it_index];
        else                dest->position = Vector3(0, 0, 0);

        if (mesh->colors)
        {
            auto c = mesh->colors[it_index];
            c.w = 1.0f; // @Cleanup: Is this redundant?

            dest->color_scale = argb_color(c);
        }
        else
        {
            dest->color_scale = 0xffffffff;
        }

        if (mesh->vertex_frames) dest->normal = mesh->vertex_frames[it_index].normal;
        else                     dest->normal = Vector3(1, 0, 0);

        if (mesh->uvs) dest->uv0 = mesh->uvs[it_index];
        else           dest->uv0 = Vector2(0, 0);

        dest->uv1 = Vector2(0, 0);
    }

    assert(dest_buffer.count);
    assert(mesh->index_array.count);

    glGenBuffers(1, &mesh->vertex_vbo);
    glGenBuffers(1, &mesh->index_vbo);
    DumpGLErrors("glGenBuffers for mesh's vertices");

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_vbo);
    DumpGLErrors("glBindBuffer for mesh's vertices");

    glBufferData(GL_ARRAY_BUFFER, sizeof(dest_buffer[0]) * count, dest_buffer.data, GL_STREAM_DRAW);
    DumpGLErrors("glBufferData for mesh's vertices");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo);
    DumpGLErrors("glBindBuffer for mesh's indices");

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(mesh->index_array[0]) * mesh->index_array.count, mesh->index_array.data, GL_STREAM_DRAW);
    DumpGLErrors("glBufferData for mesh's indices");
}

static bool load_obj_model(Triangle_Mesh *mesh)
{
    auto model_path_relative_to_exe = mesh->full_path;
    auto c_path = (char*)(temp_c_string(model_path_relative_to_exe));

    auto obj_mesh = fast_obj_read(c_path);
    defer {fast_obj_destroy(obj_mesh);};

    if (!obj_mesh) return false;

    for (auto group_index = 0; group_index < obj_mesh->group_count; ++group_index)
    {
        auto obj_group = &obj_mesh->groups[group_index];
        auto vertex_index_counter = 0;

        assert(obj_group->face_count);

        auto list_info = array_add(&mesh->triangle_list_info);
        list_info->first_index = mesh->index_array.count;
        // @Note: The 'num_indices' for the list_info will be back-fill later.

        for (auto face_index = 0; face_index < obj_group->face_count; ++face_index)
        {
            auto num_face_vertices = obj_mesh->face_vertices[obj_group->face_offset + face_index];

            //
            // Triangulate the indices.
            //
            for (auto vertex_index = 1; vertex_index <= num_face_vertices - 2; ++vertex_index)
            {
                i32 num_indices = mesh->index_array.count;
                array_add(&mesh->index_array, num_indices);
                array_add(&mesh->index_array, num_indices + vertex_index);
                array_add(&mesh->index_array, num_indices + vertex_index + 1);
            }

            //
            // Add the positions, texture coordinates, and normals.
            //
            for (auto vertex_index = 0; vertex_index < num_face_vertices; ++vertex_index)
            {
                auto face_vertex         = &obj_mesh->indices[obj_group->index_offset + vertex_index_counter];

                // For the indices, index 0 means that the value is not there. Because of this,
                // the .obj format has indices start at 1 in order to maintain this property.
                auto position_index      = face_vertex->p;
                auto texture_coord_index = face_vertex->t;
                auto normal_index        = face_vertex->n;

                if (position_index)
                {
                    auto pos = &obj_mesh->positions[3 * position_index];
                    array_add(&mesh->vertices, Vector3(pos[0], pos[1], pos[2]));
                }

                if (texture_coord_index)
                {
                    auto texcoord = &obj_mesh->texcoords[3 * texture_coord_index];
                    array_add(&mesh->uvs, Vector2(texcoord[0], texcoord[1]));
                }

                if (normal_index)
                {
                    auto normal    = &obj_mesh->normals[3 * normal_index];
                    auto frame3    = array_add(&mesh->vertex_frames);
                    frame3->normal = Vector3(normal[0], normal[1], normal[2]);
                }

                ++vertex_index_counter;
            }
        }

        list_info->num_indices = vertex_index_counter;
    }

    return true;
}

static bool load_mesh_into_memory(Triangle_Mesh *mesh)
{
    assert((mesh->full_path));
    
    auto full_path   = mesh->full_path;
    auto extension   = find_character_from_right(full_path, '.');

    if (!extension) return false; // No extension meaning that we don't know how to handle it.
    advance(&extension, 1);

    if (extension == String("obj"))
    {
        auto success = load_obj_model(mesh);
        return success;
    }
    else
    {
        assert(0);
        return false;
    }
}

void reload_asset(Mesh_Catalog *catalog, Triangle_Mesh *mesh)
{
    // auto start_time = get_time();
    // printf("Started loading mesh '%s'\n", temp_c_string(mesh->name));

    if (mesh->loaded)
    {
        logprint("mesh_catalog", "Freeing the mesh '%s'!!!!!\n", temp_c_string(mesh->full_path));
        free_resources(mesh);
    }

    auto load_success = load_mesh_into_memory(mesh);
    if (!load_success)
    {
        logprint("mesh_catalog", "Was not able to load the mesh '%s' into memory!\n", temp_c_string(mesh->full_path));
        free_resources(mesh);
        return;
    }

    make_vertex_buffer(mesh);

    // auto end_time = get_time() - start_time;
    // printf("Loaded the mesh '%s', took %f seconds!\n", temp_c_string(mesh->name), end_time);
    // newline();
}

