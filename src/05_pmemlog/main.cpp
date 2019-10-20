#include <cstring>
#include <iostream>
#include <cstdint>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <libpmemlog.h>

class close_pmemlog {
public:
  template< typename T >
  void operator()( T *p ) const {
    if( p ) pmemlog_close( p );
  }
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
  uint64_t pool_size;
  std::string filename;
  desc.add_options()
    ( "help,h", "show this message" )
    ( "create,c", "create" )
    ( "size,s", po::value< size_t >( &pool_size )->default_value( PMEMLOG_MIN_POOL ), "pool size" )
    ( "filename,f", po::value< std::string >( &filename )->default_value( "/dev/dax0.0" ), "filename" )
    ( "append,a", po::value< std::string >( &new_value ), "append" )
    ( "list,l", "list" );
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
  size_t file_size = 0u;
  bool create = vm.count( "create" );
  if( fs::exists( path ) ) {
    device_dax = is_special_file( path );
    if( !device_dax ) file_size = fs::file_size( path );
    else file_size = 0;
  }
  else {
    file_size = pool_size;
    create = true;
  }
  PMEMlogpool *raw_pool = create ?
    pmemlog_create( filename.c_str(), file_size, 0666 ) :
    pmemlog_open( filename.c_str() );
  if( !raw_pool ) {
    std::cerr << filename << ':' << strerror( errno ) << std::endl;
    return 1;
  }
  std::unique_ptr< PMEMlogpool, close_pmemlog > pool( raw_pool );
  if( !new_value.empty() )
    pmemlog_append( pool.get(), new_value.data(), new_value.size() );
  if( vm.count( "list" ) ) {
    pmemlog_walk( pool.get(), 0, []( const void *data, size_t length, void* ) -> int {
      std::cout << std::string( reinterpret_cast< const char* >( data ), length ) << std::endl;
      return 0;
    }, nullptr );
  }
}

