# Copy all DLLs from the build output directory to the install/bin directory
file(GLOB DLLS "${src_dir}/*.dll")
file(MAKE_DIRECTORY "${dst_dir}")
foreach(DLL ${DLLS})
    file(COPY "${DLL}" DESTINATION "${dst_dir}")
endforeach()
