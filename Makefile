CFLAGS=-g -D__STDC_FORMAT_MACROS -Wall -Werror -Iinclude
LIBS=-lrt -pthread

libezpoll.a:src/ez_poll.o src/ez_buffer.o src/ez_conn.o src/ez_client.o src/ez_server.o src/ez_proto.o src/ez_thread.o
	ar r $@ $^
	chmod u+x $@

src/ez_poll.o:src/ez_poll.cc include/ez_poll.h
	g++ -o $@ -c $< $(CFLAGS)

src/ez_buffer.o:src/ez_buffer.cc include/ez_buffer.h
	g++ -o $@ -c $< $(CFLAGS)

src/ez_conn.o:src/ez_conn.cc include/ez_conn.h include/ez_poll.h include/ez_buffer.h
	g++ -o $@ -c $< $(CFLAGS)

src/ez_client.o:src/ez_client.cc include/ez_conn.h include/ez_poll.h
	g++ -o $@ -c $< $(CFLAGS)

src/ez_server.o:src/ez_server.cc include/ez_conn.h include/ez_poll.h
	g++ -o $@ -c $< $(CFLAGS)

src/ez_proto.o:src/ez_proto.cc include/ez_proto.h include/ez_conn.h
	g++ -o $@ -c $< $(CFLAGS)

src/ez_thread.o:src/ez_thread.cc include/ez_thread.h include/ez_poll.h
	g++ -o $@ -c $< $(CFLAGS)

test:sample/sample_server sample/sample_client sample/sample_multi_server sample/sample_multi_client
	@echo "run samples please"

sample/sample_multi_server:sample/sample_multi_server.cc include/ez_poll.h include/ez_buffer.h libezpoll.a
	g++ -o $@ sample/sample_multi_server.cc libezpoll.a $(CFLAGS) $(LIBS)

sample/sample_multi_client:sample/sample_multi_client.cc include/ez_poll.h include/ez_buffer.h libezpoll.a
	g++ -o $@ sample/sample_multi_client.cc libezpoll.a $(CFLAGS) $(LIBS)

sample/sample_server:sample/sample_server.cc include/ez_poll.h include/ez_buffer.h libezpoll.a
	g++ -o $@ sample/sample_server.cc libezpoll.a $(CFLAGS) $(LIBS)

sample/sample_client:sample/sample_client.cc include/ez_poll.h include/ez_buffer.h libezpoll.a
	g++ -o $@ sample/sample_client.cc libezpoll.a $(CFLAGS) $(LIBS)

clean:
	rm -f src/*.o libezpoll.a sample/sample_server sample/sample_client sample/sample_multi_server sample/sample_multi_client
