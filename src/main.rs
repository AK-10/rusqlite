use std::io;
use std::process;

fn main() {
    println!("Hello, world!");

    let mut query = String::new();
    loop {
        io::stdin().read_line(&mut query).expect("failed to read line");
        if query == ".exit" {
            println!("bye✋");
            std::process::exit(0);
        } else {
            println!("Unrecognized command {}.", query);
        }
    }
}
