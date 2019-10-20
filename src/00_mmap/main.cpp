#include <cstring>
#include <iostream>
#include <cstdint>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

class unmap {
public:
  unmap( size_t len_ ) : len( len_ ) {}
  template< typename T >
  void operator()( T *p ) const {
    if( p )
      if( munmap( reinterpret_cast< void* >( p ), len ) )
        exit( 1 );
  }
private:
  size_t len;
};

namespace fs = boost::filesystem;

bool is_special_file( const fs::path &p ) {
  return
    fs::status( p ).type() == fs::file_type::character_file ||
    fs::status( p ).type() == fs::file_type::block_file;
}

int main( int argc, const char *argv[] ) {
  namespace po = boost::program_options;
  po::options_description desc( "Options" );
  std::string new_value;
  uint64_t size;
  std::string filename;
  desc.add_options()
    ( "help,h", "show this message" )
    ( "filename,f", po::value< std::string >( &filename )->default_value( "/dev/dax0.0" ), "filename" )  
    ( "write,w", po::value< std::string >( &new_value ), "write" );
  po::variables_map vm;
  po::store( po::parse_command_line( argc, argv, desc ), vm );
  po::notify( vm );
  if( vm.count( "help" ) ) {
    std::cout << desc << std::endl;
    return 0;
  }
  size_t mapped_length = 0u;
  int is_pmem = 0;
  fs::path path( filename );
  bool device_dax = false;
  size_t new_value_size = new_value.size() + 1u;
  size_t file_size = 0u;
  bool new_file = true;
  if( fs::exists( path ) ) {
    new_file = false;
    device_dax = is_special_file( path );
    if( !device_dax ) {
      size_t existing_file_size = fs::file_size( path );
      if( vm.count( "write" ) ) {
        file_size = new_value_size;
        if( existing_file_size != new_value_size ) {
          if( truncate( filename.c_str(), new_value_size ) ) {
            std::cerr << strerror( errno ) << std::endl;
            return 1;
          }
        }
      }
      else file_size = existing_file_size;
    }
  }
  else file_size = new_value_size;
  const auto fd = open( filename.c_str(), new_file ? O_RDWR|O_CREAT : O_RDWR, 0644 );
  if( fd < 0 ) {
    std::cerr << strerror( errno ) << std::endl;
    return 1;
  }
  if( new_file )
    ftruncate( fd, file_size );
  const auto raw = mmap(
    nullptr,
    file_size,
    PROT_READ|PROT_WRITE,
    MAP_SHARED,
    fd,
    0
  );
  if( raw == nullptr ) {
    std::cerr << strerror( errno ) << std::endl;
    return 1;
  }
  std::unique_ptr< char, unmap > mapped(
    reinterpret_cast< char* >( raw ),
    unmap( mapped_length )
  );
  if( vm.count( "write" ) ) {
    std::copy( new_value.begin(), new_value.end(), mapped.get() );
    mapped.get()[ new_value_size ] = '\0';
    msync( mapped.get(), file_size, MS_SYNC );
  }
  else {
    std::cout << mapped.get() << std::endl;
  }
}

