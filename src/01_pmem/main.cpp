#include <cstring>
#include <iostream>
#include <cstdint>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <libpmem.h>

class unmap_pmem {
public:
  unmap_pmem( size_t len_ ) : len( len_ ) {}
  template< typename T >
  void operator()( T *p ) const {
    if( p )
      if( pmem_unmap( reinterpret_cast< void* >( p ), len ) )
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
  if( fs::exists( path ) ) {
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
  const auto raw = pmem_map_file(
    filename.c_str(),
    file_size,
    device_dax ? 0 : PMEM_FILE_CREATE,
    0644,
    &mapped_length,
    &is_pmem
  );
  if( raw == nullptr ) {
    std::cerr << strerror( errno ) << std::endl;
    return 1;
  }
  std::unique_ptr< char, unmap_pmem > mapped(
    reinterpret_cast< char* >( raw ),
    unmap_pmem( mapped_length )
  );
  if( is_pmem )
    std::cout << filename << " is pmem." << std::endl;
  std::cout << "Mapped length : " << mapped_length << std::endl;
  if( vm.count( "write" ) ) {
    if( mapped_length < new_value_size ) {
      std::cerr << "Mapped range size must larger than data size." << std::endl;
      return 1;
    }
    std::copy( new_value.begin(), new_value.end(), mapped.get() );
    mapped.get()[ new_value_size ] = '\0';
    if( is_pmem ) {
      pmem_persist( mapped.get(), new_value_size );
    }
    else {
      if( pmem_msync( mapped.get(), new_value_size ) ) {
        std::cerr << strerror( errno ) << std::endl;
        return 1;
      }
    }
  }
  else {
    std::cout << mapped.get() << std::endl;
  }
}

