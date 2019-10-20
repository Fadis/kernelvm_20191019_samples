#include <cstring>
#include <iostream>
#include <cstdint>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <libpmemblk.h>

class close_pmemblk {
public:
  template< typename T >
  void operator()( T *p ) const {
    if( p ) pmemblk_close( p );
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
    ( "size,s", po::value< size_t >( &pool_size )->default_value( PMEMBLK_MIN_POOL ), "pool size" )
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
  constexpr size_t block_size = 1024;
  PMEMblkpool *raw_pool = create ?
    pmemblk_create( filename.c_str(), block_size, file_size, 0666 ) :
    pmemblk_open( filename.c_str(), block_size );
  if( !raw_pool ) {
    std::cerr << filename << ':' << strerror( errno ) << std::endl;
    return 1;
  }
  std::unique_ptr< PMEMblkpool, close_pmemblk > pool( raw_pool );
  const size_t block_count = pmemblk_nblock( pool.get() );
  if( create ) {
    const char buffer[ block_size ] = { 0 };
    for( size_t i = 0; i != block_count; ++i ) {
      pmemblk_write( pool.get(), buffer, i );
      if( i % ( block_count / 10 ) == 0 )
        std::cout << 100 * i / block_count << "%" << std::endl;
    }
  }
  if( !new_value.empty() ) {
    char buffer[ block_size ];
    for( size_t i = 0; i != block_count; ++i ) {
      pmemblk_read( pool.get(), buffer, i );
      if( buffer[ 0 ] == '\0' ) {
        new_value.resize( block_size - 1 );
        std::copy( new_value.begin(), new_value.end(), buffer );
	buffer[ new_value.size() ] = '\0';
        pmemblk_write( pool.get(), buffer, i );
	break;
      }
    }
  }
  if( vm.count( "list" ) ) {
    char buffer[ block_size ];
    for( size_t i = 0; i != block_count; ++i ) {
      pmemblk_read( pool.get(), buffer, i );
      if( buffer[ 0 ] == '\0' ) break;
      std::cout << buffer << std::endl;
    }
  }
}

