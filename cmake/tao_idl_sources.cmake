# Distributed under the OpenDDS License. See accompanying LICENSE
# file or http://www.opendds.org/license.html for details.

function(_opendds_tao_append_runtime_lib_dir_to_path dst)
  if(MSVC)
    set(env_var_name PATH)
  else()
    set(env_var_name LD_LIBRARY_PATH)
  endif()
  set(path_list "$ENV{${env_var_name}}" "${TAO_LIB_DIR}")
  if(TARGET OpenDDS::Util)
    list(APPEND path_list "$<TARGET_FILE_DIR:OpenDDS::Util>")
  endif()
  _opendds_path_list(path_list ${path_list})
  if(NOT MSVC)
    string(REPLACE "\\" "/" path_list "${path_list}")
  endif()
  set(${dst} "${env_var_name}=${path_list}" PARENT_SCOPE)
endfunction()

function(_opendds_get_generated_output_dir target output_dir_var)
  set(no_value_options)
  set(single_value_options O_OPT)
  set(multi_value_options)
  cmake_parse_arguments(arg
    "${no_value_options}" "${single_value_options}" "${multi_value_options}" ${ARGN})

  # TODO base output_dir_var on target
  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/opendds_generated")
  if(arg_O_OPT)
    if(IS_ABSOLUTE "${arg_O_OPT}")
      set(output_dir "${arg_O_OPT}")
    else()
      set(output_dir "${output_dir}/${arg_O_OPT}")
    endif()
  endif()
  set(${output_dir_var} "${output_dir}" PARENT_SCOPE)
endfunction()

function(_opendds_ensure_generated_output_dir target include_base file o_opt output_dir_var)
  get_filename_component(abs_file "${file}" ABSOLUTE)
  get_filename_component(abs_dir "${abs_file}" DIRECTORY)
  _opendds_get_generated_output_dir("${target}" output_dir O_OPT "${o_opt}")
  if(include_base AND NOT OPENDDS_FILENAME_ONLY_INCLUDES AND NOT O_OPT)
    get_filename_component(output_dir "${output_dir}" REALPATH)
    get_filename_component(real_abs_file "${abs_file}" REALPATH)
    get_filename_component(real_include_base "${include_base}" REALPATH)
    file(RELATIVE_PATH rel_to_output "${output_dir}" "${real_abs_file}")
    if(rel_to_output MATCHES "^\\.\\.")
      # This should be an IDL file that is relative to include_base.
      file(RELATIVE_PATH rel_file "${real_include_base}" "${real_abs_file}")
    else()
      # This should be our own generated IDL file that is relative to
      # opendds_generated.
      file(RELATIVE_PATH rel_file "${output_dir}" "${real_abs_file}")
    endif()
    get_filename_component(rel_dir "${rel_file}" DIRECTORY)
    if(rel_file MATCHES "^\\.\\.")
      message(FATAL_ERROR "This IDL file:\n\n  ${rel_file}\n\nis outside the INCLUDE_BASE:\n\n  ${include_base}")
    endif()
    if(rel_dir)
      set(output_dir "${output_dir}/${rel_dir}")
    endif()
  endif()
  file(MAKE_DIRECTORY "${output_dir}")
  set(${output_dir_var} "${output_dir}" PARENT_SCOPE)
endfunction()

function(_opendds_get_generated_file_path target include_base file output_path_var)
  _opendds_ensure_generated_output_dir(${target} "${include_base}" "${file}" "" output_dir)
  get_filename_component(filename ${file} NAME)
  set(${output_path_var} "${output_dir}/${filename}" PARENT_SCOPE)
endfunction()

function(_opendds_get_generated_idl_output
    target include_base idl_file o_opt output_prefix_var output_dir_var)
  _opendds_ensure_generated_output_dir(
    ${target} "${include_base}" "${idl_file}" "${o_opt}" output_dir)
  get_filename_component(idl_filename_no_ext ${idl_file} NAME_WE)
  set(${output_prefix_var} "${output_dir}/${idl_filename_no_ext}" PARENT_SCOPE)
  set(${output_dir_var} "${output_dir}" PARENT_SCOPE)
endfunction()

function(_opendds_tao_idl target)
  set(one_value_args AUTO_INCLUDES INCLUDE_BASE)
  set(multi_value_args IDL_FLAGS IDL_FILES)
  cmake_parse_arguments(arg "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT arg_IDL_FILES)
    message(FATAL_ERROR "called _opendds_tao_idl(${target}) without specifying IDL_FILES")
  endif()

  set(working_binary_dir ${CMAKE_CURRENT_BINARY_DIR})
  set(working_source_dir ${CMAKE_CURRENT_SOURCE_DIR})

  # convert all include paths to be relative to binary tree instead of to source tree
  file(RELATIVE_PATH rel_path_to_source_tree ${working_binary_dir} ${working_source_dir})
  foreach(flag ${arg_IDL_FLAGS})
    if("${flag}" MATCHES "^-I(\\.\\..*)")
      list(APPEND converted_flags "-I${rel_path_to_source_tree}/${CMAKE_MATCH_1}")
    else()
      list(APPEND converted_flags ${flag})
      # if the flag is like "-Wb,stub_export_file=filename" then set the variable
      # "idl_cmd_arg-wb-stub_export_file" to filename
      string(REGEX MATCH "^-Wb,([^=]+)=(.+)" m "${flag}")
      if(m)
        set(idl_cmd_arg-wb-${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
      endif()
    endif()
  endforeach()

  set(option_args -Sch -Sci -Scc -Ssh -SS -GA -GT -GX -Gxhst -Gxhsk)
  cmake_parse_arguments(idl_cmd_arg "${option_args}" "-o;-oS;-oA" "" ${arg_IDL_FLAGS})

  set(feature_flags)
  if(OPENDDS_TAO_CORBA_E_MICRO)
    list(APPEND feature_flags -DCORBA_E_MICRO -Gce)
  endif()
  if(OPENDDS_TAO_CORBA_E_COMPACT)
    list(APPEND feature_flags -DCORBA_E_COMPACT -Gce)
  endif()
  if(OPENDDS_TAO_MINIMUM_CORBA)
    list(APPEND feature_flags -DTAO_HAS_MINIMUM_POA -Gmc)
  endif()
  if(OPENDDS_TAO_NO_IIOP)
    list(APPEND feature_flags -DTAO_LACKS_IIOP)
  endif()
  if(OPENDDS_TAO_GEN_OSTREAM)
    list(APPEND feature_flags -Gos)
  endif()
  if(NOT OPENDDS_TAO_OPTIMIZE_COLLOCATED_INVOCATIONS)
    list(APPEND feature_flags -Sp -Sd)
  endif()

  if(arg_INCLUDE_BASE)
    list(APPEND converted_flags "-I${arg_INCLUDE_BASE}")
    list(APPEND auto_includes "${arg_INCLUDE_BASE}")
  endif()

  foreach(idl_file ${arg_IDL_FILES})
    set(added_output_args)
    _opendds_get_generated_idl_output(
      ${target} "${arg_INCLUDE_BASE}" "${idl_file}" "${idl_cmd_arg_-o}" output_prefix output_dir)
    list(APPEND auto_includes "${output_dir}")
    list(APPEND added_output_args "-o" "${output_dir}")
    if(idl_cmd_arg_-oS)
      _opendds_get_generated_idl_output(
        ${target} "${arg_INCLUDE_BASE}" ${idl_file} "${idl_cmd_arg_-oS}"
        skel_output_prefix skel_output_dir)
      list(APPEND auto_includes "${skel_output_dir}")
      list(APPEND added_output_args "-oS" "${skel_output_dir}")
    else()
      set(skel_output_prefix "${output_prefix}")
    endif()
    if(idl_cmd_arg_-oA)
      _opendds_get_generated_idl_output(
        ${target} "${arg_INCLUDE_BASE}" "${idl_file}" "${idl_cmd_arg_-oA}"
        anyop_output_prefix anyop_output_dir)
      list(APPEND auto_includes "${anyop_output_dir}")
      list(APPEND added_output_args "-oA" "${anyop_output_dir}")
    else()
      set(anyop_output_prefix "${output_prefix}")
    endif()

    unset(stub_header_files)
    unset(skel_header_files)
    if(NOT idl_cmd_arg_-Sch)
      set(stub_header_files "${output_prefix}C.h")
    endif()

    if(NOT idl_cmd_arg_-Sci)
      list(APPEND stub_header_files "${output_prefix}C.inl")
    endif()

    if(NOT idl_cmd_arg_-Scc)
      set(stub_cpp_files "${output_prefix}C.cpp")
    endif()

    if(NOT idl_cmd_arg_-Ssh)
      set(skel_header_files "${skel_output_prefix}S.h")
    endif()

    if(NOT idl_cmd_arg_-SS)
      set(skel_cpp_files "${skel_output_prefix}S.cpp")
    endif()

    if(idl_cmd_arg_-GA)
      set(anyop_header_files "${anyop_output_prefix}A.h")
      set(anyop_cpp_files "${anyop_output_prefix}A.cpp")
    elseif(idl_cmd_arg_-GX)
      set(anyop_header_files "${anyop_output_prefix}A.h")
    endif()

    if(idl_cmd_arg_-GT)
      list(APPEND skel_header_files
        "${skel_output_prefix}S_T.h"
        "${skel_output_prefix}S_T.cpp")
    endif()

    if(idl_cmd_arg_-Gxhst AND DEFINED idl_cmd_arg-wb-stub_export_file)
      list(APPEND stub_header_files "${CMAKE_CURRENT_BINARY_DIR}/${idl_cmd_arg-wb-stub_export_file}")
    endif()

    if(idl_cmd_arg_-Gxhsk AND DEFINED idl_cmd_arg-wb-skel_export_file)
      list(APPEND skel_header_files "${CMAKE_CURRENT_BINARY_DIR}/${idl_cmd_arg-wb-skel_export_file}")
    endif()

    get_filename_component(idl_file_path "${idl_file}" ABSOLUTE)

    set(gperf_location $<TARGET_FILE:ACE::ace_gperf>)
    if(CMAKE_CONFIGURATION_TYPES)
      get_target_property(is_gperf_imported ACE::ace_gperf IMPORTED)
      if(is_gperf_imported)
        set(gperf_location $<TARGET_PROPERTY:ACE::ace_gperf,LOCATION>)
      endif(is_gperf_imported)
    endif()

    if(BUILD_SHARED_LIB AND TARGET TAO_IDL_BE)
      set(tao_idl_shared_libs TAO_IDL_BE TAO_IDL_FE)
    endif()

    _opendds_tao_append_runtime_lib_dir_to_path(extra_lib_dirs)

    set(generated_files
      ${stub_header_files}
      ${skel_header_files}
      ${anyop_header_files}
      ${stub_cpp_files}
      ${skel_cpp_files}
      ${anyop_cpp_files}
    )
    if(debug)
      foreach(generated_file ${generated_files})
        string(REPLACE "${output_dir}/" "" generated_file "${generated_file}")
        string(REPLACE "${skel_output_dir}/" "" generated_file "${generated_file}")
        string(REPLACE "${anyop_output_dir}/" "" generated_file "${generated_file}")
        message(STATUS "tao_idl: ${generated_file}")
      endforeach()
    endif()

    set(tao_idl "$<TARGET_FILE:TAO::tao_idl>")
    if(CMAKE_GENERATOR STREQUAL "Ninja" AND TAO_IS_BEING_BUILT)
      if(CMAKE_VERSION VERSION_LESS 3.24)
        message(FATAL_ERROR "Using Ninja to build ACE/TAO requires CMake 3.24 or later. "
         "Please build ACE/TAO separately, use a newer CMake, or a different CMake generator.")
      else()
        set(tao_idl "$<PATH:ABSOLUTE_PATH,${tao_idl},\${cmake_ninja_workdir}>")
        set(gperf_location "$<PATH:ABSOLUTE_PATH,${gperf_location},\${cmake_ninja_workdir}>")
      endif()
    endif()
    add_custom_command(
      OUTPUT ${generated_files}
      DEPENDS TAO::tao_idl ${tao_idl_shared_libs} ACE::ace_gperf
      MAIN_DEPENDENCY ${idl_file_path}
      COMMAND ${CMAKE_COMMAND} -E env "DDS_ROOT=${DDS_ROOT}" "TAO_ROOT=${TAO_INCLUDE_DIR}"
        "${extra_lib_dirs}"
        "${tao_idl}" -g ${gperf_location} ${feature_flags} -Sg
        -Wb,pre_include=ace/pre.h -Wb,post_include=ace/post.h
        --idl-version 4 -as --unknown-annotations ignore
        -I${TAO_INCLUDE_DIR} -I${working_source_dir}
        ${converted_flags}
        ${added_output_args}
        ${idl_file_path}
    )

    set_property(SOURCE ${idl_file_path} APPEND PROPERTY
      OPENDDS_CPP_FILES
        ${stub_cpp_files}
        ${skel_cpp_files}
        ${anyop_cpp_files})

    set_property(SOURCE ${idl_file_path} APPEND PROPERTY
      OPENDDS_HEADER_FILES
        ${stub_header_files}
        ${skel_header_files}
        ${anyop_header_files})
  endforeach()

  if(arg_AUTO_INCLUDES)
    set("${arg_AUTO_INCLUDES}" "${auto_includes}" PARENT_SCOPE)
  endif()
endfunction()
