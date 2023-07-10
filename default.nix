{ stdenv
, cmake
, ninja
, boost
, openssl
}:

stdenv.mkDerivation {
  name = "boost_asio_awaitable_ext";
  src = ./.;
  buildInputs = [ boost openssl];
  nativeBuildInputs = [ cmake ninja ];
}
