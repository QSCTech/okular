include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}/../..
   ${TIFF_INCLUDE_DIR}
)


########### next target ###############

set(okularGenerator_tiff_SRCS
   generator_tiff.cpp
)

okular_add_generator(okularGenerator_tiff ${okularGenerator_tiff_SRCS})
target_link_libraries(okularGenerator_tiff okularcore ${TIFF_LIBRARIES} KF5::I18n)

########### install files ###############
install( FILES okularTiff.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
install( PROGRAMS okularApplication_tiff.desktop org.kde.mobile.okular_tiff.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.okular-tiff.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
