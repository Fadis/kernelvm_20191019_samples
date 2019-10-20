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
  PMEMoid next;
};

int main( int argc, const char *argv[] ) {
  namespace po = boost::program_options;
  po::options_description desc( "Options" );
  std::string new_value;
  std::string remove_value;
  uint64_t pool_size;
  constexpr const char layout[] = "90d2827d-3742-4054-aea8-7a43068085ac";
  std::string filename;
  desc.add_options()
    ( "help,h", "show this message" )
    ( "create,c", "create" )
    ( "size,s", po::value< size_t >( &pool_size )->default_value( PMEMOBJ_MIN_POOL ), "pool size" )
    ( "filename,f", po::value< std::string >( &filename )->default_value( "/dev/dax0.0" ), "filename" )
    ( "append,a", po::value< std::string >( &new_value ), "append" )
    ( "delete,d", po::value< std::string >( &remove_value ), "delete" )
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
    auto head = root;
    auto head_raw = root_raw;
    while( 1 ) {
      auto next = reinterpret_cast< data_t* >( pmemobj_direct( head_raw->next ) );
      if( next ) {
        head = head_raw->next;
        head_raw = next;
      }
      else break;
    }
    new_value.resize( std::min( new_value.size(), size_t( 1023 ) ) );
    TX_BEGIN( pool.get() ) {
      pmemobj_tx_add_range( head, offsetof( data_t, next ), sizeof( PMEMoid ) );
      PMEMoid next = pmemobj_tx_zalloc( sizeof( data_t ), 0 );
      auto next_raw = reinterpret_cast< data_t* >( pmemobj_direct( next ) );
      pmemobj_tx_add_range( next, 0, sizeof( data_t ) );
      new(next_raw) data_t();
      std::copy( new_value.begin(), new_value.end(), next_raw->message );
      next_raw->message[ new_value.size() ] = '\0';
      head_raw->next = next;
    } TX_END
  }
  if( !remove_value.empty() ) {
    auto prev = root;
    auto prev_raw = root_raw;
    auto cur = prev_raw->next;
    auto cur_raw = reinterpret_cast< data_t* >( pmemobj_direct( cur ) );
    while( cur_raw ) {
      if( strcmp( cur_raw->message, remove_value.data() ) == 0 ) {
        break;
      }
      auto next = reinterpret_cast< data_t* >( pmemobj_direct( cur_raw->next ) );
      if( next ) {
        prev = cur;
        cur = cur_raw->next;
	prev_raw = cur_raw;
        cur_raw = next;
      }
      else {
        std::cerr << "Not found." << std::endl;
	return 1;
      }
    }
    if( !cur_raw ) {
      std::cerr << "Not found." << std::endl;
      return 1;
    }
    TX_BEGIN( pool.get() ) {
      pmemobj_tx_add_range( prev, offsetof( data_t, next ), sizeof( PMEMoid ) );
      prev_raw->next = cur_raw->next;
      cur_raw->~data_t();
      pmemobj_tx_free( cur );
    } TX_END
  }
  if( vm.count( "list" ) ) {
    auto prev = root;
    auto prev_raw = root_raw;
    auto cur = prev_raw->next;
    auto cur_raw = reinterpret_cast< data_t* >( pmemobj_direct( cur ) );
    while( cur_raw ) {
      std::cout << cur_raw->message << std::endl;
      auto next = reinterpret_cast< data_t* >( pmemobj_direct( cur_raw->next ) );
      if( next ) {
        cur = cur_raw->next;
        cur_raw = next;
      }
      else break;
    }
  }
}

