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
