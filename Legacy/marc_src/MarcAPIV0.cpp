#include "MarcAPI.hpp"
//#ifdef USE_WX
    //#include "GUI/GUI.hpp"
//#endif

using namespace Marc;

// Removed duplicate implementations of methods in the file
// Corrected indentation for the entire file
MarcErrorCode MarcAPI::setActions(const StringList& actions){
    this->actions.clear();
    for (const auto& action : actions) {
        if (action == "duplicate") {
            this->actions.push_back("duplicate");
        } else if (action == "duplicate_grid") {
            this->actions.push_back("duplicate_grid");
        } else if (action == "cut") {
            this->actions.push_back("cut");
        } else if (action == "cut_x") {
            this->actions.push_back("cut_x");
        } else if (action == "cut_y") {
            this->actions.push_back("cut_y");
        } else if (action == "cut_grid") {
            this->actions.push_back("cut_grid");
        } else if (action == "split") {
            this->actions.push_back("split");
        } else if (action == "repair") {
            this->actions.push_back("repair");
        } else if (action == "info") {
            this->actions.push_back("info");
        } else if (action == "print") {
            this->actions.push_back("print");
        }
    }
    return MARC_S_OK;
}
MarcErrorCode MarcAPI::setTransforms(const StringList& transforms){
    this->actions.clear();
    for (const auto& transform : transforms) {
        if (transform == "duplicate") {
            this->actions.push_back("duplicate");
        } else if (transform == "duplicate_grid") {
            this->actions.push_back("duplicate_grid");
        } else if (transform == "cut") {
            this->actions.push_back("cut");
        } else if (transform == "cut_x") {
            this->actions.push_back("cut_x");
        } else if (transform == "cut_y") {
            this->actions.push_back("cut_y");
        } else if (transform == "cut_grid") {
            this->actions.push_back("cut_grid");
        } else if (transform == "split") {
            this->actions.push_back("split");
        } else if (transform == "repair") {
            this->actions.push_back("repair");
        } else if (transform == "info") {
            this->actions.push_back("info");
        } else if (transform == "print") {
            this->actions.push_back("print");
        }
    }
    return MARC_S_OK;
}
Marc::MarcAPI::~MarcAPI()
{
}

MarcErrorCode MarcAPI::conFigure()
{
    this->config_def.merge(cli_actions_config_def);
    this->config_def.merge(cli_transform_config_def);
    this->config_def.merge(cli_misc_config_def);
    this->config_def.merge(print_config_def);
    this->config.def = &this->config_def;

    std::cout << "Configurations Path size: " << this->configfiles_paths.size() << std::endl;
    std::cout << "Models Path  size: " << this->model_paths.size() << std::endl;
    for (const auto& file_path : this->configfiles_paths) 
    {
        std::string filename = file_path.filename().string();
        std::string name = file_path.stem().string();

        std::cout << "Trying to load config file: " << file_path.string() << std::endl;

        if (!std::filesystem::exists(file_path)) 
        {
            std::cout << "No such file: " << file_path << std::endl;
            return MARC_E_FAIL;
        }

        DynamicPrintConfig c;
        try {
            c.load(file_path.string());
        } catch (const std::exception& e) {
            //std::cerr << "Error while reading config file: " << e.what() << std::endl;
            return MARC_E_FAIL;
        } catch (...) {
            //std::cerr << "Unknown error while reading config file." << std::endl;
            return MARC_E_FAIL;
        }

        c.normalize();
        this->print_config.apply(c);
    }

    return MARC_S_OK;
}

MarcErrorCode MarcAPI::conFigure_old()
{
         /* Slic3r::ConfigDef config_def;
    Slic3r::DynamicConfig config;
    Slic3r::DynamicPrintConfig print_config;
    Slic3r::FullPrintConfig full_print_config;*/
    //std::cout << "In Configuration: "  << std::endl;
    this->config_def.merge(cli_actions_config_def);
    this->config_def.merge(cli_transform_config_def);
    this->config_def.merge(cli_misc_config_def);
    this->config_def.merge(print_config_def);
    this->config.def = &this->config_def;

    // load config files supplied via --load
    //std::cout << "Configurations size: " << this->configfiles_paths.size() << std::endl;
    for (const auto& file_path : this->configfiles_paths) 
    {
        std::string filename = file_path.filename().string();  // "my_file.txt"
        std::string name = file_path.stem().string();          // "my_file" (no extension)
        //std::string extension = filePath.extension().string();// ".txt"
		if (!std::filesystem::exists(file_path.string()))
         {
            //std::cout << "No such file: " << file_path << std::endl;
            return  MARC_E_FAIL;
         }
	
        DynamicPrintConfig c;
		//-here it reads the given .ini file and loads all the values..from file.
        try {
            c.load(file_path.string());  // -loads if *.ini file. note *.ini is value of --load command
            } catch (std::exception &e) 
            {
            //std::cerr << "Error while reading config file: " << e.what() << std::endl;
            return  MARC_E_FAIL;
            }
        c.normalize();
        this->print_config.apply(c);
    }
    return MARC_S_OK;
}
    
MarcErrorCode MarcAPI::updateModel()
{
    if (this->conFigure() != MARC_S_OK) {
        std::cerr << "Error: Failed to configure the configuration objects." << std::endl;
        return MARC_E_FAIL;
    }

    slm_model.objects.clear();  // Clear old model objects first
    Slic3r::ModelObject* modobj = slm_model.add_object();

    // Build a map from config file base names to paths for fast lookup
    std::unordered_map<std::string, std::filesystem::path> config_map;
    for (const auto& configfile : this->configfiles_paths) {
        config_map[configfile.stem().string()] = configfile;
    }

    for (const auto& file_path : this->model_paths) 
    {
        if (!std::filesystem::exists(file_path)) {
            //std::cerr << "No such file: " << file_path << std::endl;
            return MARC_E_FAIL;
        }

        Slic3r::TriangleMesh mesh;
        std::string filename = file_path.filename().string();
        std::string name = file_path.stem().string();

        try {
            mesh.ReadSTLFile(file_path.string());
            if (mesh.facets_count() == 0) {
                //std::cerr << "Error: Mesh is empty for file: " << file_path << std::endl;
                return MARC_E_FAIL;
            }

            Slic3r::ModelVolume* stlvolumeptr = modobj->add_volume(mesh);
            if (!stlvolumeptr) {
               // std::cerr << "Error: Failed to add volume for file: " << file_path << std::endl;
                return MARC_E_FAIL;
            }

            stlvolumeptr->name = filename;
            stlvolumeptr->modifier = false;

            // Attach config if available
            auto it = config_map.find(name);
            if (it != config_map.end()) {
                DynamicPrintConfig cprint_config;
                DynamicPrintConfig c = this->volumeconfig(it->second.string());
                cprint_config.apply(c);
                cprint_config.apply(config, true);
                cprint_config.normalize();
                stlvolumeptr->config = cprint_config;
                this->slm_print->apply_config(cprint_config);
            }

        } catch (const std::exception& e) {
            //std::cerr << file_path << ": " << e.what() << std::endl;
            return MARC_E_FAIL;
        }
    }

    if (slm_model.objects.empty()) {
        //std::cerr << "Error: Model is empty." << std::endl;
        return MARC_E_FAIL;
    }
    for (auto* obj : slm_model.objects) {
        if (obj->volumes.empty()) {
            //std::cerr << "Error: Object has no volumes." << std::endl;
            return MARC_E_FAIL;
        }
    }

    slm_model.add_default_instances();

    BoundingBoxf bb(Pointf(0, 0), Pointf(100, 100));
    modobj->arrange_volumes(this->spacing, &bb);

    modobj->update_bounding_box();
    modobj->center_around_origin();

    slm_model.arrange_objects(this->spacing);
    slm_model.repair();

    this->slm_print->set_model(slm_model);

    return MARC_S_OK;
}


// read input file(s) if any
 MarcErrorCode MarcAPI::updateModel_old()    //- std::vector<std::string> input_files  such as model1.stl, model2.stl.....
    //-we can add volumes in a single model.

    { //- Fistclear the objects in the model
    if (this->conFigure() != MARC_S_OK) {
        std::cerr << "Error: Failed to configure the configuration objects." << std::endl;
        return  MARC_E_FAIL;
    }

     //- read the config files  Before  ......
    slm_model.objects.clear();  // Use clear() to remove all objects safely.

    //std::vector<Slic3r::Sizef3> object_sizes;
   
    Slic3r::ModelObject* modobj = slm_model.add_object();
    //std::unique_ptr<Slic3r::ModelObject> modobj(this->slm_model.add_object());
    //Slic3r::ModelVolume* stlvolumeptr;
    //std::unique_ptr<Slic3r::ModelVolume> stlvolumeptr;
   // std::cout << "ModelPaths size: " << this->model_paths.size() << std::endl;
   std::unordered_map<std::string, std::filesystem::path> config_map;
   for (const auto& configfile : this->configfiles_paths) {
       config_map[configfile.stem().string()] = configfile;
   }



    for (const auto& file_path : this->model_paths) 
    {   // std::cout << "inside loop: "  << std::endl;

        if (!std::filesystem::exists(file_path)) {
            //std::cerr << "No such file: " << file_path << std::endl;
        return  MARC_E_FAIL;
        }
        Slic3r::ModelVolume* stlvolumeptr = nullptr;  // move inside loop
        Slic3r::TriangleMesh mesh;
        std::string filename = file_path.filename().string();  // "my_file.txt"
        std::string name = file_path.stem().string();          // "my_file" (no extension)
        //std::string extension = filePath.extension().string();// ".txt"
        try 
        {
            mesh.ReadSTLFile(file_path.string());
            if (mesh.facets_count() == 0) {
                //std::cerr << "Error: Mesh is empty or invalid for file: " << file_path << std::endl;
                return MARC_E_FAIL;
            }
            //Slic3r::ModelVolume* vol = obj->add_volume(mesh);
           
            stlvolumeptr = modobj->add_volume(mesh);
            if (!stlvolumeptr) {
                //std::cerr << "Error: Failed to add volume to the model object for file: " << file_path << std::endl;
                return  MARC_E_FAIL;
            } else {
                std::cout << "Volume added successfully to the model object." << std::endl;
            }
            
            //Slic3r::ModelInstance* instance = obj->add_instance();
            stlvolumeptr->name = filename;
            stlvolumeptr->modifier = false;
        } catch (const std::exception& e) 
        {
            //std::cerr << file_path << ": " << e.what() << std::endl;
            return  MARC_E_FAIL;  // return instead of exit()
        }

       //----------Now load inis-------------------------
       auto it = config_map.find(name);
       if (it != config_map.end()) {
           DynamicPrintConfig cprint_config;
           DynamicPrintConfig c = this->volumeconfig(it->second.string());
           cprint_config.apply(c);
           cprint_config.apply(config, true);
           cprint_config.normalize();
           stlvolumeptr->config = cprint_config;
           this->slm_print->apply_config(cprint_config);
       }
      
      //---------Just disabling it for now----------------
      /*
      for (const auto& configfile : this->configfiles_paths) 
      {
   
       // std::string name_config= config_file.substr(0, config_file.size()-4);
       std::string cfilename = configfile.filename().string();  // "my_file.txt"
       std::string cname = configfile.stem().string();          // "my_file" (no extension)
       if(cname.compare(name) == 0)
       {
  

    DynamicPrintConfig cprint_config;
    DynamicPrintConfig c =this->volumeconfig(configfile.string());
    cprint_config.apply(c);
    cprint_config.apply(config, true);
    cprint_config.normalize();
    stlvolumeptr->config = cprint_config;
    this->slm_print->apply_config(cprint_config);
           //slm_print->apply_config(printconfig);
           //std::cout << name<<  " :=: "<<name_config <<std::endl;
           //break;
      }
       
    }
*/
       //----------End inis-------------------------
      
      
    }   //  end for loop of stls
    //std::cout << "Number of objects: " << slm_model.objects.size() << std::endl;
    if (slm_model.objects.empty()) {
       // std::cerr << "Error: Model object is empty." << std::endl;
        return  MARC_E_FAIL;
    }
    for (auto* obj : slm_model.objects) {
        if (obj->volumes.empty()) {
            //std::cerr << "Error: Model object has no volumes." << std::endl;
            return  MARC_E_FAIL;
        }
    }
    
    /*std::vector<Pointf> positions = arrange(
        object_sizes,
        Sizef(bed_width, bed_depth),
        spacing
    );*/
    /*
    for (size_t i = 0; i < slm_model.objects.size(); ++i) {
        ModelObject* obj = slm_model.objects[i];
        ModelInstance* instance = obj->instances.front();
        BoundingBox bbox = obj->volumes.front()->mesh.bounding_box();

        Slic3r::Vectorf3 translation(
            positions[i].x - bbox.min.x,
            positions[i].y - bbox.min.y,
            -bbox.min.z
        );
         instance->set_translation(translation);*/
//__________________
slm_model.add_default_instances();
BoundingBoxf* bb= new  BoundingBoxf(Pointf(0,0),Pointf(100,100));
modobj->arrange_volumes(this->spacing,bb); // aThis method is used to arrange the volumes in the model object
//added byShahid in Original Modelobject class
modobj->update_bounding_box();
modobj->center_around_origin();	
//slm_model.add_default_instances();

//this->slm_model->arrange_volumes();

slm_model.arrange_objects(this->spacing);

slm_model.repair();

this->slm_print->set_model(slm_model);

//void add_model_object(ModelObject* model_object, int idx = -1);
//slm_print->add_model_object(modobj);
//std::cout << "...size of objects to..." << slm_model.objects.size() << std::endl;
//_______________


       
        return MARC_S_OK;
       

}
    


    //-Reading the model or models completes here....
    // loop through transform options

MarcErrorCode MarcAPI::duplicate_model_objects_within_bed( ) 

{
    const Slic3r::BoundingBoxf bb{ this->full_print_config.bed_shape.values };
        //for (auto &model : this->slm_models) {
    const bool all_objects_have_instances = std::none_of(
    slm_model.objects.begin(), slm_model.objects.end(),
    [](Slic3r::ModelObject* o){ return o->instances.empty(); }
            );
    if (all_objects_have_instances) {
                // if all input objects have defined position(s) apply duplication to the whole model
        slm_model.duplicate(this->config.getInt("duplicate"), this->full_print_config.min_object_distance(), &bb);
        } else {
        slm_model.add_default_instances();
        slm_model.duplicate_objects(this->config.getInt("duplicate"), this->full_print_config.min_object_distance(), &bb);
        }
        
}



// Center all objects of the model around a point `P` (in XY)
MarcErrorCode MarcAPI::center_model_objects_around_point( const Slic3r::Pointf& P)
{
    
    slm_model.add_default_instances();
        // this affects instances:
    slm_model.center_instances_around_point(P);
        // this affects volumes:
    slm_model.align_to_ground();
    return MARC_S_OK;
}


 
        //else if (opt_key == "duplicate") 
MarcErrorCode MarcAPI::duplicate()
{
    const Slic3r::BoundingBoxf bb{ this->full_print_config.bed_shape.values };
           
        const bool all_objects_have_instances = std::none_of(
        slm_model.objects.begin(), slm_model.objects.end(),
        [](Slic3r::ModelObject* o){ return o->instances.empty(); }
        );
        if (all_objects_have_instances) {
                    // if all input objects have defined position(s) apply duplication to the whole model
            slm_model.duplicate(this->config.getInt("duplicate"), this->full_print_config.min_object_distance(), &bb);
            } else {
            slm_model.add_default_instances();
            slm_model.duplicate_objects(this->config.getInt("duplicate"), this->full_print_config.min_object_distance(), &bb);
            }
            return MARC_S_OK;
 } 
 //else if (opt_key == "duplicate_grid")
MarcErrorCode MarcAPI::duplicate_grid()       
{
    auto &ints = this->config.opt<Slic3r::ConfigOptionInts>("duplicate_grid")->values;
    const int x = ints.size() > 0 ? ints.at(0) : 1;
    const int y = ints.size() > 1 ? ints.at(1) : 1;
    const double distance = this->full_print_config.duplicate_distance.value;
            //for (auto &model : this->slm_models)
    slm_model.duplicate_objects_grid(x, y, (distance > 0) ? distance : 6);  // TODO: this is not the right place for setting a default
    return MARC_S_OK;
} 
       // else if (opt_key == "center")
MarcErrorCode MarcAPI::around_center() 

{
    this->slm_model.add_default_instances();
                // this affects instances:
    this->slm_model.center_instances_around_point(Slic3r:: Pointf(0,0));
                // this affects volumes:
    this->slm_model.align_to_ground();
    return MARC_S_OK;
            
}
MarcErrorCode MarcAPI::alignXY(const float& px,const float& py) 
{  
    
            //const Pointf p{ this->config.opt<ConfigOptionPoint>("align_xy")->value };
    Slic3r::BoundingBoxf3 bb{ slm_model.bounding_box() };
                // this affects volumes:
    slm_model.translate(-(bb.min.x - px), -(bb.min.y - py), -bb.min.z);
    return MARC_S_OK;
} 
            
MarcErrorCode MarcAPI::rotateZ(const float& angle)
{
    for (auto &o : slm_model.objects)
                    // this affects volumes:
        o->rotate(Geometry::deg2rad(angle), Z);
        return MARC_S_OK;
} 
MarcErrorCode MarcAPI::rotateX(const float& angle) 
{
    for (auto &o : slm_model.objects)
                    // this affects volumes:
        o->rotate(Geometry::deg2rad(angle), X);
        return MARC_S_OK;
} 
MarcErrorCode MarcAPI::rotateY(const float& angle) 
{      
    for (auto &o : slm_model.objects)
                    // this affects volumes:
        o->rotate(Geometry::deg2rad(angle), Y);
        return MARC_S_OK;
} 
MarcErrorCode MarcAPI::scale(const float& s) 
{
    for (auto &o : slm_model.objects)
                    // this affects volumes:
        o->scale(s);
        return MARC_S_OK;
} 
        
MarcErrorCode MarcAPI::scaleToFit() 
{
          /*  const auto opt = config.opt<ConfigOptionPoint3>(opt_key);
            if (!opt->is_positive_volume()) {
                Slic3r::Log::error("CLI") << "--scale-to-fit requires a positive volume" << std::endl;
                exit(EXIT_FAILURE);
            }
          
                for (auto &o : slm_model.objects)
                    // this affects volumes:
                    o->scale_to_fit(opt->value);*/
                    return MARC_E_NOTIMPL; 
} 
      
        //else if (opt_key == "cut" || opt_key == "cut_x" || opt_key == "cut_y")
MarcErrorCode MarcAPI::cut(std::string opt_key,float xx ,std::vector<Model>& new_models)  
{
            //std::vector<Model> new_models;
            
    slm_model.repair();
    slm_model.translate(0, 0, -slm_model.bounding_box().min.z);  // align to z = 0
    Slic3r::Model out;
        for (auto &o : slm_model.objects) {
            if (opt_key == "cut_x") {
                o->cut(X, xx, &out);
            } else if (opt_key == "cut_y") {
                o->cut(Y, xx, &out);
            } else if (opt_key == "cut") {
                o->cut(Z, xx, &out);
            }
                }
                
                // add each resulting object as a distinct model
    if (out.objects.size() >= 2) {
        Slic3r::Model upper, lower;
        auto upper_obj = upper.add_object(*out.objects[0]);
        auto lower_obj = lower.add_object(*out.objects[1]);
        if (upper_obj && upper_obj->facets_count() > 0) new_models.push_back(upper);
        if (lower_obj && lower_obj->facets_count() > 0) new_models.push_back(lower);
    } else {
        Slic3r::Log::error("CLI") << "Cut operation did not produce enough objects." << std::endl;
    }
            
            
            // TODO: copy less stuff around using pointers
            //this->models = new_models;
            
    //if (this->actions.empty())
        this->actions.push_back("export_stl");
        return MARC_S_OK;
} 
        
        
       // else if (opt_key == "cut_grid") 
MarcErrorCode MarcAPI::cut_grid(std::string opt_key,std::vector<Model>& new_models)      
{
            //std::vector<Model> new_models;
             
    Slic3r::TriangleMesh mesh = this->slm_model.mesh();
    mesh.repair();
            
    Slic3r::TriangleMeshPtrs meshes = mesh.cut_by_grid(config.opt<Slic3r::ConfigOptionPoint>("cut_grid")->value);
    size_t i = 0;
    for (const auto& m : meshes) {
        Slic3r::Model out;
        auto o = out.add_object();
        o->add_volume(*m);
        o->input_file += "_" + std::to_string(i++);
    }
            
            
            // TODO: copy less stuff around using pointers
           // this->models = new_models;
            
    //if (this->actions.empty())
        this->actions.push_back("export_stl");
        return MARC_S_OK;
} 
        
       // else if (opt_key == "split") 
MarcErrorCode MarcAPI::split()  
{
         
    this->slm_model.split();
    return MARC_S_OK;
} 
       // else if (opt_key == "repair") 
        
MarcErrorCode MarcAPI::repair()        
{
           
    this->slm_model.repair();
    return MARC_S_OK;
}


    
    // loop through action options
   // for (auto const &opt_key : this->actions) 

    
    //else if (opt_key == "save") 
MarcErrorCode MarcAPI::save_conf()   
{
    this->print_config.save(config.getString("save"));
    return MARC_S_OK;
} 
    
// else if (opt_key == "info")
MarcErrorCode MarcAPI::info() 
{
            // --info works on unrepaired model      
    this->slm_model.add_default_instances();
    this->slm_model.print_info();
    return MARC_S_OK;
            
} 








MarcErrorCode MarcAPI::exportslasvg()
         {     this->updateModel();
            //for (const Model &model : this->models) {
                Slic3r::SLAPrint print(&slm_model); // initialize print with model
                print.config.apply(this->print_config, true); // apply configuration
                print.slice(); // slice file
                const std::string outfile = this->output_filepath(slm_model, IO::SVG);
                print.write_svg(outfile); // write SVG
                std::cout << "SVG file exported to " << outfile << std::endl;
                return MARC_S_OK;
               /* std::unique_ptr<SLAPrint> print(new SLAPrint(&model));
                print->config.apply(this->print_config, true); // apply configuration
                print->slice(); // slice file
                const std::string outfile = this->output_filepath(model, IO::SVG);
                print->write_svg(outfile); // write SVG
                boost::nowide::cout << "SVG file exported to " << outfile << std::endl;*/
    //print->to_marc();  will port this method to marc...
                //Marc::ExportMarcSlice conversion(std::move(print));

           // }
        } 
        
        
        
        //else if (opt_key == "export_gcode") 

MarcErrorCode MarcAPI::exportSlmFile() {
    // Step 1: Update the model
    if (this->updateModel() != MARC_S_OK) {
        //std::cerr << "Error: Failed to update the model." << std::endl;
        return  MARC_E_FAIL;
    }

    // Step 2: Apply configuration
    this->slm_print->apply_config(this->print_config);

    // Step 3: Validate configuration
  
    // Step 4: Slice the model
   

   

    // Step 6: Export SVG layers
    try {
        this->slm_print->write_svg_layers();
    } catch (std::runtime_error& e) {
        //std::cerr << "Error: Failed to export SVG layers: " << e.what() << std::endl;
        return  MARC_E_FAIL;
    }

    std::cout << "SLM SVG exported successfully." << std::endl;
    return MARC_S_OK;
}
       // else if (opt_key == "print") 
MarcErrorCode MarcAPI::print()
{   return MARC_E_NOTIMPL; 
   
}
    
    
     
    


MarcErrorCode MarcAPI::print_help(bool include_print_options) 
{
    std::cout
        << "Slic3r " << SLIC3R_VERSION << " (build commit: " << BUILD_COMMIT << ")" << std::endl
        << "https://slic3r.org/ - https://github.com/slic3r/Slic3r" << std::endl << std::endl
        << "Usage: slic3r [ ACTIONS ] [ TRANSFORM ] [ OPTIONS ] [ file.stl ... ]" << std::endl
        << std::endl
        << "Actions:" << std::endl;
    cli_actions_config_def.print_cli_help(std::cout, false);
    
    std::cout
        << std::endl
        << "Transform options:" << std::endl;
        cli_transform_config_def.print_cli_help(std::cout, false);
    
    std::cout
        << std::endl
        << "Other options:" << std::endl;
        cli_misc_config_def.print_cli_help(std::cout, false);
    
    if (include_print_options) {
        std::cout << std::endl;
        print_config_def.print_cli_help(std::cout, true);
    } else {
        std::cout
            << std::endl
            << "Run --help-options to see the full listing of print/G-code options." << std::endl;
    }
    return MARC_S_OK;
}

/// @brief 
/// @param format 
MarcErrorCode MarcAPI::export_model(IO::ExportFormat format) 
{
    
        const std::string outfile = this->output_filepath(this->slm_model, format);
        
        IO::write_model.at(format)(slm_model, outfile);
        std::cout << "File exported to " << outfile << std::endl;
        return MARC_S_OK;
    
}

void Marc::MarcAPI::registerProgressCallback(ProgressCallback cb)
{
}

std::string
MarcAPI::output_filepath(const Model &model, IO::ExportFormat format) const 
{
    // get the --output-filename-format option
    std::string filename_format = this->print_config.getString("output_filename_format", "[input_filename_base]");
    
    // strip the file extension and add the correct one
    filename_format = filename_format.substr(0, filename_format.find_last_of("."));
    filename_format += "." + IO::extensions.at(format);
    
    // this is the same logic used in Print::output_filepath()
    // TODO: factor it out to a single place?
    
    // find the first input_file of the model
    std::filesystem::path input_file;
    for (auto o : slm_model.objects) {
        if (!o->input_file.empty()) {
            input_file = o->input_file;
            break;
        }
    }
    
    // compute the automatic filename
    PlaceholderParser pp;
    pp.set("input_filename", input_file.filename().string());
    pp.set("input_filename_base", input_file.stem().string());
    pp.apply_config(config);
    const std::string filename = pp.process(filename_format);
    
    // use --output when available
    std::string outfile{ config.getString("output", "") };
    if (!outfile.empty()) {
        // if we were supplied a directory, use it and append our automatically generated filename
        const std::filesystem::path out(outfile);
        if (std::filesystem::is_directory(out))
            outfile = (out / filename).string();
    } else {
        outfile = (input_file.parent_path() / filename).string();
    }
    
    return outfile;
}

Slic3r::DynamicPrintConfig  MarcAPI::volumeconfig(std::string file)
{    	
if (!std::filesystem::exists(file)) {
            std::cout << "No such file: " << file << std::endl;
            exit(1);
        }
	 DynamicPrintConfig c;
		// Here it reads the given .ini file and loads all the values..from file.
        try {
            c.load(file);  //  loads if *.ini file. note *.ini is value 0f --load command
        } catch (std::exception &e) {
            std::cout << "Error while reading config file: " << e.what() << std::endl;
            exit(1);
        }
        c.normalize();
        //print_config.apply(c);
    return c;

}

#include <filesystem>
#include <iostream>

MarcErrorCode MarcAPI::setConfigs(const PathList& _configfiles_paths) {
    // Check if any paths are empty or invalid before processing
    if (_configfiles_paths.empty()) {
        //std::cerr << "No config files provided." << std::endl;
        return MARC_E_FAIL;
    }

    // Loop through all paths and check if they exist
    for (const auto& path : _configfiles_paths) {
        // Validate path is not empty and exists
        if (path.empty()) {
            //std::cerr << "Empty path found in config list." << std::endl;
            return MARC_E_FAIL;
        }
        
        // Try to check if the file exists
        try {
            if (!std::filesystem::exists(path)) {
                //std::cerr << "Config file does not exist: " << path << std::endl;
                return MARC_E_FAIL;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // Catch any filesystem errors (e.g., malformed paths or permission issues)
            //std::cerr << "Filesystem error when checking path " << path << ": " << e.what() << std::endl;
            return MARC_E_FAIL;
        } catch (...) {
            //std::cerr << "Unknown error occurred while checking path: " << path << std::endl;
            return MARC_E_FAIL;
        }
    }

    // Move the config paths into the member variable
    this->configfiles_paths = std::move(_configfiles_paths);
    //std::cout << "Configurations Path size at setConfigs: " << this->configfiles_paths.size() << std::endl;
    // Log the size of the config files list
   // std::cout << "out Configpaths size: " << this->configfiles_paths.size() << std::endl;

    return MARC_S_OK;
}


MarcErrorCode MarcAPI::setModels(const PathList&  _mesh_paths)
{
    for (const auto& path : _mesh_paths) {
       
        if (path.empty()) {
           // std::cerr << "Empty model path provided." << std::endl;
            return MARC_E_FAIL;
        }
       
       
        try {
            if (!std::filesystem::exists(path)) {
                //std::cerr << "Model file does not exist: " << path << std::endl;
                return MARC_E_FAIL;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            //std::cerr << "Filesystem error checking path: " << path << "\n"
                      //<< "Error: " << e.what() << std::endl;
            return MARC_E_FAIL;
        } catch (...) {
           // std::cerr << "Unknown error checking path: " << path << std::endl;
            return MARC_E_FAIL;
        }
    }

    this->model_paths = std::move(_mesh_paths);
    //std::cout << "Out ModelPaths size: " << this->model_paths.size() << std::endl;

    return MARC_S_OK;
}
