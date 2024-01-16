cmake_minimum_required(VERSION 3.22)

set(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/out)

project(AudioPlugin VERSION 0.1.0)

juce_add_plugin(${PROJECT_NAME}    
    IS_SYNTH FALSE
    COMPANY_NAME Jeff Blake
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE 
    PLUGIN_CODE AUPL 
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "Modular_Delay"
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "C:/Program Files/Common Files/VST3/" 
)

target_sources(${PROJECT_NAME}
    PRIVATE
        source/PluginEditor.cpp
        source/PluginProcessor.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE 
        include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        juce::juce_audio_utils
    PUBLIC 
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
    )
endif()