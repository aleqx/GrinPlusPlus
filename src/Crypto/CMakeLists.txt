set(TARGET_NAME Crypto)

if(MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
			string(REGEX REPLACE "/W[0-4]" "/W2 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W2 /WX")
    endif()
endif()

file(GLOB SOURCE_CODE
	"AggSig.cpp"
	"Bulletproofs.cpp"
	"Crypto.cpp"
	"Pedersen.cpp"
	"PublicKeys.cpp"
	"RandomNumberGenerator.cpp"
	"ThirdParty/Blake2b.cpp"
	"ThirdParty/sha256.cpp"
	"ThirdParty/sha512.cpp"
	"ThirdParty/hmac_sha256.cpp"
	"ThirdParty/hmac_sha512.cpp"
	"ThirdParty/ripemd160.cpp"
	"ThirdParty/aes.cpp"
)

if(GRINPP_STATIC)
	add_library(${TARGET_NAME} STATIC ${SOURCE_CODE})
else(GRINPP_STATIC)
	add_library(${TARGET_NAME} SHARED ${SOURCE_CODE})
endif(GRINPP_STATIC)

target_compile_definitions(${TARGET_NAME} PRIVATE MW_CRYPTO)

add_dependencies(${TARGET_NAME} Infrastructure ctaes scrypt secp256k1-zkp)
target_link_libraries(${TARGET_NAME} PUBLIC Infrastructure ctaes scrypt secp256k1-zkp)