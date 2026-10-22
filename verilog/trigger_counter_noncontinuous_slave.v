//alinhar ciclos  
`default_nettype none

module top (
    input  wire        SCK,
    input  wire        clki,
    input  wire        rst,
    input  wire        CS,
    output wire        led1,
    output wire        led2,
    input  wire [11:0] trg,
    output reg         out, 
    output wire        MISO,
    input  wire        MOSI
  );

  wire        out1;
  wire        out2;
  wire        read;
  wire full_t1;
  wire full_t2;
  wire full_time;
  wire [11:0] full_ch;
  wire reciving;

  wire [31:0] evtcount_t1;
  wire [31:0] evtcount_t2;
  wire [31:0] timecount;
  
  wire [9:0]  mode;

  wire [2:0]  level;
  reg  [2:0]  leveli = 3'd0;
  reg  [15:0] sync_in = 16'd0;

  reg  [11:0] trg_sync0 = 12'd0;
  reg  [11:0] trg_sync1 = 12'd0;
  reg  [11:0] trg_sync01 = 12'd0;
  reg  [11:0] trg_sync02 = 12'd0;
  reg  [11:0] trg_sync03 = 12'd0;
  reg  [11:0] trg_sync04 = 12'd0;
  reg  [11:0] trg_sync05 = 12'd0;
  reg  [11:0] trg_sync06 = 12'd0;
  reg  [11:0] trg_sync07 = 12'd0;

  // Synchronizes reset with the main clock before distributing it.
  reg reciving0 = 1'b0, reciving1 = 1'b0, reciving2 = 1'b0;
  reg mode_rst0 = 1'b0, mode_rst1 = 1'b0;
  always @(posedge clki) begin
    reciving0 <= reciving;              // two stages against metastability
    reciving1 <= reciving0;
    reciving2 <= reciving1;            // one more, to find the edge
    mode_rst0 <= mode[8];
    mode_rst1 <= mode_rst0;             // two stages against metastability
  end
  wire end_reciving = ~reciving1 && reciving2;
  wire rsti = ~rst || (mode_rst1 && end_reciving);
  reg [1:0] rst_pipe = 2'b11;
  always @(posedge clki or posedge rsti)
  begin
    if (rsti) rst_pipe <= 2'b11;
    else     rst_pipe <= {rst_pipe[0], 1'b0};
  end
  wire rst_sync = rst_pipe[1];

  
  
  trigger_1 trigger_1_1(
  .leveli  (leveli),
  .sync_in (sync_in),
  .clki    (clki),
  .out1    (out1)
  );


  // Generates a trigger when a window of three active channels exists.
  trigger_2 trigger_2_1(
    .trg  (trg_sync02),
    .clki (clki),
    .out  (out2)
  );
 



  // Receives the 9-bit configuration sent by the SPI controller.
  receive_spi receive_spi_1(
    .MOSI        (MOSI),
    .SCK         (SCK),
    .CS          (CS),
    .mode        (mode),
    .reciving    (reciving)
  );


  reg full_timei = 1'b0;
  reg full_t1i = 1'b0;
  reg full_t2i = 1'b0;



  // Counts events from both trigger types.
  wire readt1 = (full_t1i) ? read : 1'b0;
  counter countert_1 (
    .rst     (rst_sync),
    .trigger (out1),
    .clki    (clki),
    .evtcount(evtcount_t1),
    .full    (full_t1),
    .read     (readt1)
  );
  wire readt2 = (full_t2i) ? read : 1'b0;
  counter countert_2 (
    .rst     (rst_sync),
    .trigger (out2),
    .clki    (clki),
    .evtcount(evtcount_t2),
    .full    (full_t2),
    .read     (readt2)
  );


  // One independent counter for each input channel.
  genvar i;
  wire [31:0] ch_count [0:11]; 
  wire read_ch [0:11];
  reg [9:0] mode_sync2;
  reg [3:0] seli = 4'd0;
  wire [3:0] sel = mode_sync2[3:0];
  reg CS_sync1 = 1'b0, CS_sync2 = 1'b0, CS_sync3 = 1'b0;
  reg full_chi = 1'b0;

  always @(posedge clki)
  begin
    CS_sync1 <= CS;
    CS_sync2 <= CS_sync1;
    CS_sync3 <= CS_sync2;
    if (CS_sync2) begin
      seli<= sel;
      if (sel < 4'd12) full_chi <= full_ch[sel];
      else full_chi <= 1'b0; 
      full_timei <= full_time;
      full_t1i <= full_t1;
      full_t2i <= full_t2;
    end
  end
  generate

      for (i = 0; i < 12; i = i + 1) begin : gen_cnt
          assign read_ch[i] = (full_chi && seli == i) ? read : 1'b0;
          counter u_cnt (
              .rst      (rst_sync),
              .trigger  (trg_sync1[i]),
              .clki     (clki),
              .evtcount (ch_count[i]),
              .full     (full_ch[i]),
              .read     (read_ch[i])
          );
      end
  endgenerate

 

  // Millisecond timer based on the clki frequency.
  wire read_time = (full_timei) ? read : 1'b0;
  ms_counter ms_counter1 (
    .rst      (rst_sync),
    .clki     (clki),
    .timecount(timecount),
    .full     (full_time),
    .read     (read_time)
  );




  reg [2:0]  leveli1;
  // Synchronizes inputs and selects the output trigger.
  always @(posedge clki)
  begin
    if (CS_sync2 && !CS_sync3) begin
        mode_sync2 <= mode; // Atualização única e segura, sem estados fantasmas.
      end
    leveli1 <= level;
    leveli <= leveli1;
    trg_sync0 <= trg;
    //syncronizes with the trigger_1 ouitput, which has a 10-cycle delay.
    trg_sync01 <= trg_sync0;
    trg_sync02 <= trg_sync01;
    trg_sync03 <= trg_sync02;
    trg_sync04 <= trg_sync03;
    trg_sync05 <= trg_sync04;
    trg_sync06 <= trg_sync05;
    trg_sync07 <= trg_sync06;
    trg_sync1 <= trg_sync07;

    sync_in <= {2'b0, trg_sync01[11:6],2'b0, trg_sync01[5:0]};
    if (mode_sync2[7]) out <= out2;
    else out <= out1;
  end
  
  // Displays the configured level on the LEDs.
  assign level = mode_sync2[6:4];
  assign led1 = level[0];
  assign led2 = level[1];
  //assign led3 = level[2];
 
  wire [31:0] evtcount = (sel < 4'd12) ? ch_count[sel] : 32'd0;
 
  // Sends 144 bits: the two event counters of the triggers, the counter of the selected channel, and the elapsed time.
  send_spi #(144) send_counte_spi_1 (
    .evtcountE ({evtcount_t1, evtcount_t2, evtcount, timecount ,full_t1, full_t2, full_ch ,full_time, 1'b1}), 
    .clki      (clki),
    .SCK       (SCK),
    .CS_sync  (CS_sync2),
    .CS        (CS),
    .MISO      (MISO),
    .read      (read)
  );

endmodule

module count8 (
    input  wire [7:0] data,
    input  wire       clki,
    output reg  [3:0] count
  );

  reg [7:0] s1;
  reg [7:0] s2;
  always @(posedge clki)
  begin
    s1 <= (data & 8'h55) + ((data >> 1) & 8'h55);
    s2 <= (s1 & 8'h33)   + ((s1 >> 2)   & 8'h33);
    count <= s2[7:4] + s2[3:0];
  end
  
endmodule
module trigger_1 (
  input wire  [2:0]  leveli,
  input  wire [15:0] sync_in,
  input  wire        clki,
  output reg         out1


);
// Counts the active bits in each half of the input.
  wire [3:0] c8_out1;
  wire [3:0] c8_out2;
  count8 cnt8_1(
    .clki (clki),
    .data (sync_in[15:8]),
    .count(c8_out1)
  );
  count8 cnt8_2(
    .clki (clki),
    .data (sync_in[7:0]),
    .count(c8_out2)
  );

  reg  [4:0] c8_out;
  reg        trigger;

  // Adds both groups and compares the result with the configured level.
  always @(posedge clki) begin
    c8_out <= c8_out1 + c8_out2;
    trigger <= c8_out > (5'd0 + leveli);
    out1 <= trigger;
  end
endmodule

// Increments a counter on the trigger's 0 to 1 transition.
module counter (
    input  wire        rst,
    input  wire        trigger,
    input  wire        clki,
    output reg  [31:0] evtcount = 32'd0,
    output reg         full = 1'b0,
    input  wire        read
  );
  reg  trigger_1;
  // Detects the trigger rising edge and counts one event per pulse.
  wire trigger_count = trigger && !trigger_1;
  reg [32:0] evtcount_next = 33'd0;
  always @(posedge clki)
  begin
    trigger_1 <= trigger;
  end
  always @(posedge clki or posedge rst)
  begin
    if (rst) begin
      evtcount <= 32'd0;
      full <= 1'b0;
      evtcount_next <= 33'd0;
    end else begin
      if (trigger_count) begin

        if (evtcount_next[32]) begin
          full <= 1'b1;
          evtcount <= 32'd0;
          evtcount_next <= 33'd0;
        end else begin
          evtcount <= evtcount + 1'b1;
          evtcount_next <= {1'b0, evtcount} + 33'd2;
        end 
      end
      if (read) begin
        full <= 1'b0;
      end
    end
  end
endmodule

// Counts milliseconds from the clki clock.
module ms_counter #(
  parameter CLK_FREQ_HZ = 100_000_000,
  parameter CYCLES_PER_MS = CLK_FREQ_HZ / 1000,
  parameter width = $clog2(CYCLES_PER_MS),
  parameter ALIGN_CYCLES  = 'd10, //syncronizes with the trigger_1 ouitput, which has a 10-cycle delay.
  parameter cwidth        = $clog2(ALIGN_CYCLES + 1)
)(
input wire        rst,
input wire        clki,
output reg [31:0] timecount = 32'd0,
output reg full = 1'b0,
input wire read
);
 
 reg [width-1:0] mscount = 'd0;
 reg [cwidth-1:0] c = 'd0;
 always @(posedge clki or posedge rst)
  begin
    if (rst) begin
      timecount <= 32'd0;
      mscount <= 'd0;
      full <= 1'b0;
      c <= 'd0;

    end else begin
    
      // When one millisecond is reached, reset the short counter.
      if (mscount == CYCLES_PER_MS-1) begin
        timecount <= timecount + 1'b1;
        mscount <= 'd0;
        if (timecount == 32'hFFFFFFFF) begin
          full <= 1'b1;
        end
      end else begin
        //syncronizes with the trigger_1 ouitput, which has a 10-cycle delay.
        if (c == ALIGN_CYCLES) begin
          mscount <= mscount + 1'b1;
        end 
      end
      if (c < ALIGN_CYCLES) c <= c + 1'b1;
   
      
      if (read) begin
        full <= 1'b0;
      end
    end

  
  end

endmodule

// SPI slave transmitter: shifts the most significant bit onto MISO first.
module send_spi #(
  parameter WIDTH = 16
)(
  input  wire [WIDTH-1:0] evtcountE,
  input  wire             clki,
  input  wire             SCK,
  input  wire             CS, 
  input  wire             CS_sync, //the CS signal is synchronized with the clki clock in the top module, so it can be used directly in clki domain here.
  output wire             MISO,
  output wire              read
  );
  reg [WIDTH-1:0] evtcount = {WIDTH{1'b0}};
  reg readi = 1'd0;
  reg read0 = 1'b0;
  reg read1 = 1'b0;
  reg read2 = 1'b0;
  // Captures the value to transmit during the SPI transaction.
  always @(posedge clki)
  begin
    if (CS_sync) evtcount<= evtcountE;
  end
 
  // Mask that advances one bit on each falling edge of SCK.
  reg [WIDTH-1:0] n = {1'b1, {WIDTH-1{1'b0}}};


  // Resets the mask at the start of a transaction and shifts it while reading.
  always @(negedge SCK or posedge CS)
  begin
  
   if (CS) begin
    n <= {1'b1, {WIDTH-1{1'b0}}};
   end else if (!CS) begin
    n <= {1'b0, n[WIDTH-1:1]};
   end

  end
  always @(posedge SCK)
  begin
    readi <= n[0];
  end

  wire MISO_interno = |(evtcount & n);
  assign MISO = (!CS) ? MISO_interno : 1'bz;

  always @(posedge clki)
  begin
    read0 <= readi;
    read1 <= read0;
    read2 <= read1;
  end
  assign read = read1 && !read2;
endmodule


// SPI configuration word:
// mode[3:0]                - channel selected for reading.
// mode[6:4]                - majority trigger level.
// mode[7]                  - trigger type: 0 for majority, 1 for trigger_2.
// mode[8]                  - reset
// mode[9]                  - first bit of the SPI message.
// SPI slave receiver: collects an 9-bit word from MOSI.
module receive_spi (
input wire MOSI,
input wire SCK,
input wire CS,
output reg [9:0] mode = 10'd0,
output reg reciving = 1'd0
);
  reg [9:0] modei=10'd0;
  reg [8:0] n=9'd0;

  // CS resets reception; each rising edge of SCK receives one bit.
  always @(posedge SCK or posedge CS)
  begin
    if (CS) begin
      reciving <= 1'd0;
      n <= 9'd0;
      modei <= 10'd0;
    end else begin
      if (MOSI & (n==9'd0)) begin
        modei <= {9'd0, MOSI};
        n <= 9'd1;
      end else if (n[8]) mode <= {modei[8:0], MOSI}; 
      if (|n)  begin
        n <= {n[7:0], 1'd0};
        modei <= {modei[8:0], MOSI};
        reciving <= 1'd1;
      end
    end
   

  end



endmodule

// Generates a trigger when any window of three consecutive channels
// is active at the same time.
module trigger_2 (
  input  wire [11:0] trg,
  input  wire        clki,
  output reg        out
);
wire outi;
reg outi0 = 1'b0;
reg outi01 = 1'b0;
reg outi1 = 1'b0;
reg outi02 = 1'b0;
reg outi03 = 1'b0;

// Results for the ten possible windows within the 12 channels.
wire A;
wire B;
wire C;
wire D;
wire E;
wire F;
wire G;
wire H;
wire I;
wire J;

assign A = &trg[2:0];
assign B = &trg[3:1];
assign C = &trg[4:2];
assign D = &trg[5:3];
assign E = &trg[6:4];
assign F = &trg[7:5];
assign G = &trg[8:6];
assign H = &trg[9:7];
assign I = &trg[10:8];
assign J = &trg[11:9];


// One valid window is enough to activate the combined trigger.
assign outi = |{A,B,C,D,E,F,G,H,I,J};

// amostrates and synchronizes the result with clki.
always @(posedge clki)
begin
  outi0 <= outi;
  //syncronizes with the trigger_1 output, which has a 10-cycle delay.
  outi01 <= outi0;
  outi02 <= outi01;
  outi03 <= outi02;
  outi1 <= outi03;
  out  <= outi1;
end  

endmodule 

`default_nettype wire

