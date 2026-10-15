include(CMakePackageConfigHelpers)

set(rfc_base_targets ${RFC_EXPORT_TARGETS})
list(REMOVE_ITEM rfc_base_targets rfc_vulkan_work)
install(TARGETS ${rfc_base_targets} EXPORT RFCTargets)
install(EXPORT RFCTargets NAMESPACE rfc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RFC)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/rfc
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")
if(RFC_ENABLE_VULKAN)
  install(TARGETS rfc_vulkan_binary rfc_vulkan_work EXPORT RFCVulkanTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(EXPORT RFCVulkanTargets NAMESPACE rfc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RFC)
endif()
if(RFC_ENABLE_SQLITE_INTERPRETATION)
  install(TARGETS rfc_sqlite_interpretation EXPORT RFCSQLiteTargets)
  install(EXPORT RFCSQLiteTargets NAMESPACE rfc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RFC)
endif()
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RFCConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/RFCConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RFC)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/RFCConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/RFCConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/RFCConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RFC)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/rfc)

# Keep the installed user guides together so their relative links work offline.
set(rfc_documentation_dir "${CMAKE_INSTALL_DATADIR}/doc/rfc")
install(FILES README.md LICENSE DESTINATION "${rfc_documentation_dir}")
file(GLOB_RECURSE rfc_user_guides CONFIGURE_DEPENDS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "docs/README.md" "examples/README.md")
foreach(guide IN LISTS rfc_user_guides)
  get_filename_component(guide_directory "${guide}" DIRECTORY)
  install(FILES "${guide}" DESTINATION "${rfc_documentation_dir}/${guide_directory}")
endforeach()
