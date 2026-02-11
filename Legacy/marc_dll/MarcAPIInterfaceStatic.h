#ifndef MARC_API_INTERFACE_HPP
#define MARC_API_INTERFACE_HPP

#include <cstddef>
#include <cstdint>

#ifdef _WIN32
    #ifdef MARC_STATIC_DEFINE
        #define MARC_API
    #else
        #ifdef MARC_API_EXPORTS
            #define MARC_API __declspec(dllexport)
        #else
            #define MARC_API __declspec(dllimport)
        #endif
    #endif
#else
    #define MARC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int MarcErrorCode;

#define MARC_S_OK        0x00000000
#define MARC_E_FAIL      0x80004005
#define MARC_E_INVALID   0x80070057
#define MARC_E_NOTIMPL   0x80004001

MARC_API const char* get_last_error_message();

typedef void (*MarcProgressCallback)(const char* message, int percent);

struct MarcPathArray {
    const char** paths;
    std::size_t count;
};

typedef void* MarcHandle;

MARC_API MarcHandle create_marc_api(float bed_width, float bed_depth, float spacing);
MARC_API void destroy_marc_api(MarcHandle handle);

MARC_API MarcErrorCode set_models(MarcHandle handle, MarcPathArray models);
MARC_API MarcErrorCode set_configs(MarcHandle handle, MarcPathArray configs);
MARC_API MarcErrorCode set_actions(MarcHandle handle, const char** actions, std::size_t count);
MARC_API MarcErrorCode set_transforms(MarcHandle handle, const char** transforms, std::size_t count);

MARC_API MarcErrorCode update_model(MarcHandle handle);
MARC_API MarcErrorCode duplicate(MarcHandle handle);
MARC_API MarcErrorCode duplicate_grid(MarcHandle handle);
MARC_API MarcErrorCode around_center(MarcHandle handle);
MARC_API MarcErrorCode align_xy(MarcHandle handle, float px, float py);
MARC_API MarcErrorCode rotate_x(MarcHandle handle, float angle);
MARC_API MarcErrorCode rotate_y(MarcHandle handle, float angle);
MARC_API MarcErrorCode rotate_z(MarcHandle handle, float angle);
MARC_API MarcErrorCode scale(MarcHandle handle, float s);
MARC_API MarcErrorCode scale_to_fit(MarcHandle handle);
MARC_API MarcErrorCode split(MarcHandle handle);
MARC_API MarcErrorCode repair(MarcHandle handle);
MARC_API MarcErrorCode info(MarcHandle handle);
MARC_API MarcErrorCode print(MarcHandle handle);

MARC_API MarcErrorCode export_slm_file(MarcHandle handle);
MARC_API void register_progress_callback(MarcHandle handle, MarcProgressCallback callback);

#ifdef __cplusplus
}
#endif

#endif // MARC_API_INTERFACE_HPP
