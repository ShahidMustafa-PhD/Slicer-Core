	

//#include "mainwindow.h"
//#include <QApplication>
//#include <crtdbg.h>
// Include filesystem FIRST, before other headers
#if __cplusplus >= 201703L
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem::v1;
#endif
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cstdlib>

#include "../marcAPI_manager/MarcAPIManager.hpp" 
#ifdef _WIN32
  #include <conio.h>
#endif

/**
 * @file Main.cpp
 * @brief Entry point for the application demonstrating the usage of the MarcAPI.
 *
 * This program creates an instance of the MarcAPI using a factory function, sets up
 * models, configurations, actions, and transformations, and performs operations such
 * as updating the model, duplicating it, and printing its state. Finally, it cleans up
 * by destroying the API instance.
 *
 * @details
 * - The MarcAPI instance is created with initial parameters (200.0f, 200.0f).
 * - Models, actions, transformations, and configurations are set using respective methods.
 * - The `updateModel`, `duplicate`, and `print` methods demonstrate the API's functionality.
 * - Proper cleanup is ensured by calling `Marc::destroy_marc_api` to release resources.
 *
 * @return int Returns 0 upon successful execution.
 */
 int main() {
        // Create
    //MarcPathArray model_paths;
    //model_paths.paths = modPaths;
    //model_paths.count = modCount;

    //MarcPathArray config_paths;
    //config_paths.paths = cofPaths;
    //config_paths.count = cofCount;
   
        //"C:/Projects/DMLS_DLL_Test/build/bunny.STL",
      
        // "C:/Projects/DMLSV_DLL/Test/femure.STL",
           //"C:/Projects/DMLSV_DLL/Test/bridge2.STL"
               //model_paths.paths = modPaths1;
    //model_paths.count = 2;


   
        //"C:/Projects/DMLS_DLL_Test/build/bunny.ini",
    
       //"C:/Projects/DMLSV_DLL/Test/femure.ini" ,
      //"C:/Projects/DMLSV_DLL/Test/build/S.ini",
       //"C:/Projects/DMLSV_DLL/Test/bridge2.ini"
        

    
std::vector<fs::path> model_paths;
fs::path filePath  = "C:/Projects/DMLSV_DLL_V4.0/Test/Models/STLBMS.STL";
model_paths.push_back(filePath);
filePath = "C:/Projects/DMLSV_DLL_V4.0/Test/Models/femure.stl";
model_paths.push_back(filePath);
 //filePath = "C:/Projects/DMLSV_DLL_V2.2/Test/Models/bridge2.STL";
//model_paths.push_back(filePath);


std::vector<fs::path> config_paths; 
filePath =  "C:/Projects/DMLSV_DLL_V4.0/Test/Models/marc_build_config.json";
config_paths.push_back(filePath);
//filePath = "C:/Projects/DMLSV_DLL_V2.0/Test/Models/bridge2.ini";
//config_paths.push_back(filePath);
//filePath = "C:/Projects/DMLSV_DLL_V2.2/Test/Models/bridge2.ini";
//config_paths.push_back(filePath);
//std::vector<fs::path> styles_paths; 
//filePath =  "C:/Projects/DMLSV_DLL_V4.0/Test/Models/marc_build_styles.json";
//styles_paths.push_back(filePath);

/*
struct InternalModel 
        {
        std::string path;
        std::string config;
        int number;
        double xpos, ypos, zpos;
        double roll, pitch, yaw;
        };*/
std::vector<Marc::InternalModel> models;
for (size_t i = 0; i < model_paths.size(); ++i) {
    Marc::InternalModel model;
    model.path = model_paths[i].string();
   // model.config = config_paths[i].string();
    model.number = static_cast<int>(i + 1); // Assign a unique number starting from 1
    model.xpos = 0.0; // Default position
    model.ypos = 0.0; // Default position
    model.zpos = 0.0; // Default position
    model.roll = 0.0; // Default orientation
    model.pitch = 0.0; // Default orientation
    model.yaw = 0.0; // Default orientation
    models.push_back(model);
}

//std::cout << "Models size: " << models.size() << std::endl;

//Only the first model will have a config assigned for demonstration purposes
if (!config_paths.empty() && !models.empty())
{models[0].buildconfig = config_paths[0].string(); 
//models[0].stylesconfig = styles_paths[0].string();
}// Assign the first config to the first model

Marc::MarcAPIManager manager(100.0f, 100.0f, 5.0f);
//MarcAPI(100, 100, float spacing = 5.0f)

manager.setModels(models);
//manager.setConfigs(config_paths);
//manager.updateModel();
manager.exportSlmFile();
    //manager.save_conf();
    //manager.repair();
    //manager.info();
    //manager.print();
    //manager.exportslasvg();
    //manager.exportSlmFile();
    // wait for user input so console window doesn't disappear
    #ifdef _WIN32
        std::cout << "Press any key to exit...";
        _getch();
    #else
        std::cout << "Press Enter to exit...";
        std::cin.get();
    #endif

    return 0;
    }
    

