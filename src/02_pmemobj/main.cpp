#include <cstring>
#include <iostream>
#include <cstdint>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <libpmemobj.h>

class close_pmemobj {
public:
  template< typename T >
  void operator()( T *p ) const {
    if( p ) pmemobj_close( p );
  }
};

namespace fs = boost::filesystem;

bool is_special_file( const fs::path &p ) {
  return
    fs::status( p ).type() == fs::file_type::character_file ||
    fs::status( p ).type() == fs::file_type::block_file;
}

struct data_t {
  char message[ 1024 ];
};

int main( int argc, const char *argv[] ) {
  namespace po = boost::program_options;
  po::options_description desc( "Options" );
  std::string new_value;
  uint64_t pool_size;
  constexpr const char layout[] = "295a2784-1816-4fb3-9496-f1534313bac9";
  std::string filename;
  desc.add_options()
    ( "help,h", "show this message" )
    ( "create,c", "create" )
    ( "size,s", po::value< size_t >( &pool_size )->default_value( PMEMOBJ_MIN_POOL ), "pool size" )
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
  PMEMobjpool *raw_pool = create ?
    pmemobj_create( filename.c_str(), layout, file_size, 0666 ) :
    pmemobj_open( filename.c_str(), layout );
  if( !raw_pool ) {
    std::cerr << filename << ':' << strerror( errno ) << std::endl;
    return 1;
  }
  std::unique_ptr< PMEMobjpool, close_pmemobj > pool( raw_pool );
  PMEMoid root = pmemobj_root( pool.get(), sizeof( data_t ) );
  auto root_raw = reinterpret_cast< data_t* >( pmemobj_direct( root ) );
  if( !new_value.empty() ) {
    new_value.resize( std::min( new_value.size(), size_t( 1023 ) ) );
    TX_BEGIN( pool.get() ) {
      pmemobj_tx_add_range( root, offsetof( data_t, message ), sizeof( char ) * ( new_value.size() + 1 ) );
      std::copy( new_value.begin(), new_value.end(), root_raw->message );
      root_raw->message[ new_value.size() ] = '\0';
    } TX_END
  }
  else std::cout << root_raw->message << std::endl;
}

