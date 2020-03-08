mod meta_command {
    #[derive(Debug)]
    enum MetaCommandResult {
        Success,
        UnrecognizedCommand,
    }

    #[derive(Debug)]
    struct MetaCommand {}

    impl MetaCommand {
            pub fn process(command: String) -> MetaCommandResult {
            if command == ".exit" {
                // .exit
                MetaCommandResult::Success
            } else {
                MetaCommandResult::UnrecognizedCommand
            }
        }
    }
}
