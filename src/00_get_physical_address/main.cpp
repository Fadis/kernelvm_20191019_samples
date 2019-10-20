#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <cstdint>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/phoenix.hpp>
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
struct map_t {
  map_t() : begin( 0 ), end( 0 ) {};
  map_t( uint64_t b, uint64_t e ) : begin( b ), end( e ) {};
  uint64_t begin;
  uint64_t end;
};
using maps_t = boost::container::flat_multimap< std::string, map_t >;
template< typename Iterator >
class maps_parser : public boost::spirit::qi::grammar< Iterator, maps_t() > {
public:
  maps_parser() : maps_parser::base_type( root ) {
    namespace qi = boost::spirit::qi;
    namespace phx = boost::phoenix;
    elem = (
      uint_p >> '-' >> uint_p >> qi::omit[ +qi::standard::blank ] >>
      qi::omit[ qi::repeat( 4 )[ +qi::standard::graph >> +qi::standard::blank ] ] >>
      qi::as_string[ *( qi::char_ - qi::eol ) ]
    )[ qi::_val = phx::construct< maps_t::value_type >(
      qi::_3, phx::construct< map_t >( qi::_1, qi::_2 )
    ) ];
    root = elem[ phx::insert( qi::_val, phx::end( qi::_val ), qi::_1 ) ] % qi::eol;
  }
private:
  boost::spirit::qi::uint_parser< uint64_t, 16, 1, 16 > uint_p;
  boost::spirit::qi::rule< Iterator, maps_t::value_type() > elem;
  boost::spirit::qi::rule< Iterator, maps_t() > root;
};

struct page_map_entry_t {
  page_map_entry_t( uint64_t value ) :
    page_frame_number( 0 ), swap_type( 0 ), swap_offset( 0 ),
    soft_dirty( false ), exclusive( false ), file_page( false ), swapped( false ), present( false ) 
  {
    if( ( value >> 62 ) & 0x1 ) {
      swap_type = value & 0xF;
      swap_offset = ( value >> 4 ) & 0x3FFFFFFFFFFFFull;
      swapped = true;
    }
    else {
      page_frame_number = ( value & 0x3FFFFFFFFFFFFFull ) * getpagesize();
      swapped = false;
    }
    soft_dirty = ( value >> 55 ) & 0x1;
    exclusive = ( value >> 56 ) & 0x1;
    file_page = ( value >> 61 ) & 0x1;  
    present = ( value >> 63 ) & 0x1;  
  }
  uint64_t page_frame_number;
  uint8_t swap_type;
  uint64_t swap_offset;
  bool soft_dirty;
  bool exclusive;
  bool file_page;
  bool swapped;
  bool present;
};

page_map_entry_t get_physical_address( const std::string &pid, uint64_t virt_addr ) {
  namespace fs = boost::filesystem;
  const auto pagemap_path = fs::path( "/proc" ) / fs::path( pid ) / fs::path( "pagemap" );
  if( !fs::exists( pagemap_path ) ) {
    std::cout << "Process not found" << std::endl;
    return 0;
  }
  const int fd = open( pagemap_path.c_str(), O_RDONLY );
  if( fd < 0 ) {
    std::cerr << "Unable to open pagemap" << std::endl;
    return 0;
  }
  const auto offset = virt_addr / getpagesize() * 8;
  char buf[ 8 ] = { 0 };
  pread( fd, reinterpret_cast< void* >( buf ), 8, offset );
  namespace qi = boost::spirit::qi;
  auto iter = buf;
  const auto end = std::next( buf, 8 );
  uint64_t phys_addr = 0u;
  qi::parse( iter, end, qi::qword, phys_addr );
  if( ( phys_addr >> 62 ) & 0x1 ) {
    std::cout << "Page is swapped" << std::endl;
    return 0;
  }
  return page_map_entry_t( phys_addr );
}

int main( int argc, const char *argv[] ) {
  namespace po = boost::program_options;
  po::options_description desc( "Options" );
  std::string filename;
  std::string pid;
  desc.add_options()
    ( "help,h", "show this message" )
    ( "filename,f", po::value< std::string >( &filename ), "file name" )  
    ( "pid,p", po::value< std::string >( &pid ), "process id" );
  po::variables_map vm;
  po::store( po::parse_command_line( argc, argv, desc ), vm );
  po::notify( vm );
  if( vm.count( "help" ) || !vm.count( "filename" ) || !vm.count( "pid" ) ) {
    std::cout << desc << std::endl;
    return 0;
  }
  namespace fs = boost::filesystem;
  const auto maps_path = fs::path( "/proc" ) / fs::path( pid ) / fs::path( "maps" );
  if( !fs::exists( maps_path ) ) {
    std::cout << "Process not found" << std::endl;
    return 1;
  }
  std::fstream file( maps_path.c_str(), std::ios::in );
  if( !file.good() < 0 ) {
    std::cerr << "Unable to open maps" << std::endl;
    return 1;
  }
  const auto serialized_maps = std::string(
    std::istreambuf_iterator<char>( file ),
    std::istreambuf_iterator<char>()
  );
  maps_t maps;
  {
    auto iter = serialized_maps.begin();
    auto end = serialized_maps.end();
    maps_parser< std::string::const_iterator > rule;
    if( !boost::spirit::qi::parse( iter, end, rule, maps ) ) {
      std::cout << "Unable to parse maps" << std::endl;
      return 1;
    }
  }
  const auto match = maps.equal_range( filename );
  std::for_each( match.first, match.second, [&pid,&filename]( const maps_t::value_type &v ) {
    auto entry = get_physical_address( pid, v.second.begin );
    if( entry.swapped ) 
      std::cout << filename << ": VirtualAddress=0x" << std::hex << v.second.begin << " SwapType=0x" << entry.swap_type  << " SwapOffset=0x" << entry.swap_offset;
    else
      std::cout << filename << ": VirtualAddress=0x" << std::hex << v.second.begin << " PhysicalAddress=0x" << entry.page_frame_number;
    if( entry.soft_dirty ) std::cout << " D";
    if( entry.exclusive ) std::cout << " E";
    if( entry.file_page ) std::cout << " F";
    if( entry.swapped ) std::cout << " S";
    if( entry.present ) std::cout << " P";
    std::cout << std::endl;
  } );
}

