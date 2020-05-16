describe "database" do
  before do
    `rm -rf test.db`
  end

  def run_script(commands)
    raw_output = nil
    # サブプロセスを実行
    IO.popen("./db test.db", "r+") do |pipe|
      commands.each do |command|
        pipe.puts command
      end

      pipe.close_write

      # 出力全部を読み込む
      raw_output = pipe.gets(nil)
    end
    raw_output.split("\n")
  end

  it "inserts and retrives a row" do
    result = run_script([
      "insert 1 user1 person1@example.com",
      "select",
      ".exit"
    ])

    expect(result).to match_array([
      "db > Executed.",
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > "
    ])
  end


  # pageサイズ: 4096
  # rowサイズ: 291
  # 最大page数 100
  # 4096 / 291 * 100 = 1400
  # 1401でtable full error
  it "prints error message when table is full" do

    script = (1..1401).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end

    script << ".exit"
    result = run_script(script)
    
    expect(result[-2]).to eq("db > Error: Table full.")
  end

  it "arrrows inserting strings that are the maximum length" do
    long_username = "a"*32
    long_email = "a"*255

    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit"
    ]

    result = run_script(script)
    expect(result).to match_array([
      "db > Executed.",
      "db > (1, #{long_username}, #{long_email})",
      "Executed.",
      "db > "
    ])
  end

  it "prints error message if strings are too long" do
    long_username = "a"*33
    long_email = "a"*256

    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit"
    ]

    result = run_script(script)

    expect(result).to match_array([
      "db > String is too long.",
      "db > Executed.",
      "db > ",
    ])
  end

  it "prints an error message if id is negative" do
    script = [
      "insert -1 cstack foo@bar.com",
      "select",
      ".exit"
    ]

    result = run_script(script)

    expect(result).to match_array([
      "db > ID must be positive.",
      "db > Executed.",
      "db > ",
    ])
  end

  it "keep data after closing connection" do
    result1 = run_script([
      "insert 1 user1 person1@example.com",
      ".exit",
    ])
    expect(result1).to match_array([
      "db > Executed.",
      "db > "
    ])

    result2 = run_script([
      "select",
      ".exit"
    ])
    expect(result2).to match_array([
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > ",
    ])
  end
end
