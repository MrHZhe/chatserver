#把.指定路径下所有源文件名字放入变量名SRC_LIST里面
aux_source_directory(. SRC_LIST)
#表示生成可执行文件，由SRC_LIST变量所定义的源文件编译而来
add_executable(ChatClient ${SRC_LIST})
#指定可执行文件链接时需要依赖的库文件
target_link_libraries(ChatClient pthread)