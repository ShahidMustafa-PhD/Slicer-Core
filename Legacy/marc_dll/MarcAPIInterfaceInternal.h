// Internal representation of a model (example)
#pragma once


#include <vector>
#include <string>




  namespace Marc {

struct InternalModel 
        {
        std::string path;
        std::string buildconfig;
        //std::string stylesconfig;
        int number;
        double xpos, ypos, zpos;
        double roll, pitch, yaw;
        };


}