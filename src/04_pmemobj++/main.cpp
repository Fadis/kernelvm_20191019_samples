#include <iostream>
#include <cstdint>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

using pmem::obj::p;
using pmem::obj::persistent_ptr;
struct data_t {
  persistent_ptr< data_t > next;
  p< std::array< char, 1024 > > data;
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
  std::string new_value = "";
  std::string remove_value = "";
  uint64_t pool_size;
  constexpr const char layout[] = "dd58d49d-4be6-44e0-b160-37e79d94ecf8";
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
    exit( 0 );
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
  namespace pobj = pmem::obj;
  auto pool = create ?
    pobj::pool< data_t >::create( filename.c_str(), layout, file_size, 0666 ) :
    pobj::pool< data_t >::open( filename.c_str(), layout );
  pobj::persistent_ptr< data_t > root = pool.get_root();
  if( !new_value.empty() ) {
    auto next = root->next;
    auto cur = root;
    while( next ) {
      cur = next;
      next = next->next;
    }
    new_value.resize( 1023 );
    std::array< char, 1024 > data;
    std::copy( new_value.begin(), new_value.end(), data.begin() );
    data[ 1023 ] = '\0';
    pmem::obj::transaction::exec_tx( pool, [&] {
      auto new_elem = pmem::obj::make_persistent< data_t >();
      new_elem->data = data;
      cur->next = new_elem;
    } );
  }
  if( !remove_value.empty() ) {
    auto next = root->next;
    auto cur = root;
    while( next ) {
      if( strcmp( next->data.get_ro().data(), remove_value.data() ) == 0 ) {
        const auto data_size = strlen( next->data.get_ro().data() );
        pmem::obj::transaction::exec_tx( pool, [&] {
          cur->next = next->next;
          pmem::obj::delete_persistent< data_t >( next );
        } );
        break;
      }
      cur = next;
      next = next->next;
    }
  }
  if( vm.count( "list" ) ) {
    auto next = root->next;
    while( next ) {
      std::cout << next->data.get_ro().data() << std::endl;
      next = next->next;
    }
  }
}
