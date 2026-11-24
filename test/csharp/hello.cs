class Hello {
  static int Main() {
    System.Console.WriteLine(Greet.Who("mkn"));
    return Greet.Who("mkn") == "hello mkn" ? 0 : 1;
  }
}
