{ stdenv
, cmake
, ninja
, boost
, git
}:

stdenv.mkDerivation {
  name = "boost_asio_awaitable_ext";
  src = ./.;
  buildInputs = [ boost ];
  nativeBuildInputs = [ cmake ninja git];
}
