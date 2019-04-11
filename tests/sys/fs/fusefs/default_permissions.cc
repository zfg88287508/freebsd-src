/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * This software was developed by BFF Storage Systems, LLC under sponsorship
 * from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Tests for the "default_permissions" mount option.  They must be in their own
 * file so they can be run as an unprivileged user
 */

extern "C" {
#include <sys/types.h>
#include <sys/extattr.h>

#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class DefaultPermissions: public FuseTest {

virtual void SetUp() {
	m_default_permissions = true;
	FuseTest::SetUp();
	if (HasFatalFailure() || IsSkipped())
		return;

	if (geteuid() == 0) {
		GTEST_SKIP() << "This test requires an unprivileged user";
	}
	
	/* With -o default_permissions, FUSE_ACCESS should never be called */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_ACCESS);
		}, Eq(true)),
		_)
	).Times(0);
}

public:
void expect_getattr(uint64_t ino, mode_t mode, uint64_t attr_valid, int times,
	uid_t uid = 0)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_GETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = mode;
		out->body.attr.attr.size = 0;
		out->body.attr.attr.uid = uid;
		out->body.attr.attr_valid = attr_valid;
	})));
}

void expect_lookup(const char *relpath, uint64_t ino, mode_t mode,
	uint64_t attr_valid, uid_t uid = 0)
{
	FuseTest::expect_lookup(relpath, ino, mode, 0, 1, attr_valid, uid);
}

};

class Access: public DefaultPermissions {};
class Lookup: public DefaultPermissions {};
class Open: public DefaultPermissions {};
class Setattr: public DefaultPermissions {};
class Unlink: public DefaultPermissions {};

/* 
 * Test permission handling during create, mkdir, mknod, link, symlink, and
 * rename vops (they all share a common path for permission checks in
 * VOP_LOOKUP)
 */
class Create: public DefaultPermissions {
public:

void expect_create(const char *relpath, uint64_t ino)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in->body.bytes +
				sizeof(fuse_open_in);
			return (in->header.opcode == FUSE_CREATE &&
				(0 == strcmp(relpath, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, create);
		out->body.create.entry.attr.mode = S_IFREG | 0644;
		out->body.create.entry.nodeid = ino;
		out->body.create.entry.entry_valid = UINT64_MAX;
		out->body.create.entry.attr_valid = UINT64_MAX;
	})));
}

};

class Deleteextattr: public DefaultPermissions {
public:
void expect_removexattr()
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_REMOVEXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));
}
};

class Getextattr: public DefaultPermissions {
public:
void expect_getxattr(ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_GETXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}
};

class Listextattr: public DefaultPermissions {
public:
void expect_listxattr()
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_LISTXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto i __unused, auto out) {
		out->body.listxattr.size = 0;
		SET_OUT_HEADER_LEN(out, listxattr);
	})));
}
};

class Rename: public DefaultPermissions {
public:
	/* 
	 * Expect a rename and respond with the given error.  Don't both to
	 * validate arguments; the tests in rename.cc do that.
	 */
	void expect_rename(int error)
	{
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in->header.opcode == FUSE_RENAME);
			}, Eq(true)),
			_)
		).WillOnce(Invoke(ReturnErrno(error)));
	}
};

class Setextattr: public DefaultPermissions {
public:
void expect_setxattr(int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_SETXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)));
}
};

TEST_F(Access, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = X_OK;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);

	ASSERT_NE(0, access(FULLPATH, access_mode));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Access, eacces_no_cached_attrs)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = X_OK;

	expect_getattr(1, S_IFDIR | 0755, 0, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0);
	expect_getattr(ino, S_IFREG | 0644, 0, 1);
	/* 
	 * Once default_permissions is properly implemented, there might be
	 * another FUSE_GETATTR or something in here.  But there should not be
	 * a FUSE_ACCESS
	 */

	ASSERT_NE(0, access(FULLPATH, access_mode));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Access, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = R_OK;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);
	/* 
	 * Once default_permissions is properly implemented, there might be
	 * another FUSE_GETATTR or something in here.
	 */

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

TEST_F(Create, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1);
	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, ino);

	fd = open(FULLPATH, O_CREAT | O_EXCL, 0644);
	EXPECT_LE(0, fd) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Create, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_EQ(-1, open(FULLPATH, O_CREAT | O_EXCL, 0644));
	EXPECT_EQ(EACCES, errno);
}

TEST_F(Deleteextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);

	ASSERT_EQ(-1, extattr_delete_file(FULLPATH, ns, "foo"));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Deleteextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	expect_removexattr();

	ASSERT_EQ(0, extattr_delete_file(FULLPATH, ns, "foo"))
		<< strerror(errno);
}

/* Delete system attributes requires superuser privilege */
TEST_F(Deleteextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, geteuid());

	ASSERT_EQ(-1, extattr_delete_file(FULLPATH, ns, "foo"));
	ASSERT_EQ(EPERM, errno);
}

/* Deleting user attributes merely requires WRITE privilege */
TEST_F(Deleteextattr, user)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, 0);
	expect_removexattr();

	ASSERT_EQ(0, extattr_delete_file(FULLPATH, ns, "foo"))
		<< strerror(errno);
}

TEST_F(Getextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	char data[80];
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0600, UINT64_MAX, 0);

	ASSERT_EQ(-1,
		extattr_get_file(FULLPATH, ns, "foo", data, sizeof(data)));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Getextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	char data[80];
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;
	ssize_t r;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	/* Getting user attributes only requires read access */
	expect_lookup(RELPATH, ino, S_IFREG | 0444, UINT64_MAX, 0);
	expect_getxattr(
		ReturnImmediate([&](auto in __unused, auto out) {
			memcpy((void*)out->body.bytes, value, value_len);
			out->header.len = sizeof(out->header) + value_len;
		})
	);

	r = extattr_get_file(FULLPATH, ns, "foo", data, sizeof(data));
	ASSERT_EQ(value_len, r)  << strerror(errno);
	EXPECT_STREQ(value, data);
}

/* Getting system attributes requires superuser privileges */
TEST_F(Getextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	char data[80];
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, geteuid());

	ASSERT_EQ(-1,
		extattr_get_file(FULLPATH, ns, "foo", data, sizeof(data)));
	ASSERT_EQ(EPERM, errno);
}

TEST_F(Listextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0600, UINT64_MAX, 0);

	ASSERT_EQ(-1, extattr_list_file(FULLPATH, ns, NULL, 0));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Listextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1);
	/* Listing user extended attributes merely requires read access */
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);
	expect_listxattr();

	ASSERT_EQ(0, extattr_list_file(FULLPATH, ns, NULL, 0))
		<< strerror(errno);
}

/* Listing system xattrs requires superuser privileges */
TEST_F(Listextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1);
	/* Listing user extended attributes merely requires read access */
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());

	ASSERT_EQ(-1, extattr_list_file(FULLPATH, ns, NULL, 0));
	ASSERT_EQ(EPERM, errno);
}

/* A component of the search path lacks execute permissions */
TEST_F(Lookup, eacces)
{
	const char FULLPATH[] = "mountpoint/some_dir/some_file.txt";
	const char RELDIRPATH[] = "some_dir";
	uint64_t dir_ino = 42;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELDIRPATH, dir_ino, S_IFDIR | 0700, UINT64_MAX, 0);

	EXPECT_EQ(-1, access(FULLPATH, F_OK));
	EXPECT_EQ(EACCES, errno);
}

TEST_F(Open, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);

	EXPECT_NE(0, open(FULLPATH, O_RDWR));
	EXPECT_EQ(EACCES, errno);
}

TEST_F(Open, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDONLY);
	EXPECT_LE(0, fd) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Rename, eacces_on_srcdir)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDST[] = "d/dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX);
	EXPECT_LOOKUP(1, RELDST)
		.Times(AnyNumber())
		.WillRepeatedly(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Rename, eacces_on_dstdir_for_creating)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDSTDIR[] = "d";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t src_ino = 42;
	uint64_t dstdir_ino = 43;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, src_ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDSTDIR, dstdir_ino, S_IFDIR | 0755, UINT64_MAX);
	EXPECT_LOOKUP(dstdir_ino, RELDST).WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Rename, eacces_on_dstdir_for_removing)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDSTDIR[] = "d";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t src_ino = 42;
	uint64_t dstdir_ino = 43;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, src_ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDSTDIR, dstdir_ino, S_IFDIR | 0755, UINT64_MAX);
	EXPECT_LOOKUP(dstdir_ino, RELDST).WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Rename, eperm_on_sticky_srcdir)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 01777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX);

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EPERM, errno);
}

TEST_F(Rename, eperm_on_sticky_dstdir)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDSTDIR[] = "d";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t src_ino = 42;
	uint64_t dstdir_ino = 43;
	uint64_t dst_ino = 44;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, src_ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDSTDIR, dstdir_ino, S_IFDIR | 01777, UINT64_MAX);
	EXPECT_LOOKUP(dstdir_ino, RELDST)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = dst_ino;
		out->body.entry.attr_valid = UINT64_MAX;
		out->body.entry.entry_valid = UINT64_MAX;
		out->body.entry.attr.uid = 0;
	})));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EPERM, errno);
}

/* Successfully rename a file, overwriting the destination */
TEST_F(Rename, ok)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	// The inode of the already-existing destination file
	uint64_t dst_ino = 2;
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1, geteuid());
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDST, dst_ino, S_IFREG | 0644, UINT64_MAX);
	expect_rename(0);

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}

TEST_F(Rename, ok_to_remove_src_because_of_stickiness)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 01777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	EXPECT_LOOKUP(1, RELDST).WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_rename(0);

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}

TEST_F(Setattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, geteuid());
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.mode = S_IFREG | newmode;
	})));

	EXPECT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);
}

TEST_F(Setattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in->header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).Times(0);

	EXPECT_NE(0, chmod(FULLPATH, newmode));
	EXPECT_EQ(EPERM, errno);
}

TEST_F(Setextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;
	ssize_t r;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	expect_setxattr(0);

	r = extattr_set_file(FULLPATH, ns, "foo", (void*)value, value_len);
	ASSERT_EQ(value_len, r) << strerror(errno);
}

TEST_F(Setextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);

	ASSERT_EQ(-1,
		extattr_set_file(FULLPATH, ns, "foo", (void*)value, value_len));
	ASSERT_EQ(EACCES, errno);
}

// Setting system attributes requires superuser privileges
TEST_F(Setextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, geteuid());

	ASSERT_EQ(-1,
		extattr_set_file(FULLPATH, ns, "foo", (void*)value, value_len));
	ASSERT_EQ(EPERM, errno);
}

// Setting user attributes merely requires write privileges
TEST_F(Setextattr, user)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;
	ssize_t r;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, 0);
	expect_setxattr(0);

	r = extattr_set_file(FULLPATH, ns, "foo", (void*)value, value_len);
	ASSERT_EQ(value_len, r) << strerror(errno);
}

TEST_F(Unlink, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0777, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	expect_unlink(1, RELPATH, 0);

	ASSERT_EQ(0, unlink(FULLPATH)) << strerror(errno);
}

/*
 * Ensure that a cached name doesn't cause unlink to bypass permission checks
 * in VOP_LOOKUP.
 *
 * This test should pass because lookup(9) purges the namecache entry by doing
 * a vfs_cache_lookup with ~MAKEENTRY when nameiop == DELETE.
 */
TEST_F(Unlink, cached_unwritable_directory)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	EXPECT_LOOKUP(1, RELPATH)
	.Times(AnyNumber())
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto i __unused, auto out) {
			SET_OUT_HEADER_LEN(out, entry);
			out->body.entry.attr.mode = S_IFREG | 0644;
			out->body.entry.nodeid = ino;
			out->body.entry.entry_valid = UINT64_MAX;
		}))
	);

	/* Fill name cache */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	/* Despite cached name , unlink should fail */
	ASSERT_EQ(-1, unlink(FULLPATH));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Unlink, unwritable_directory)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());

	ASSERT_EQ(-1, unlink(FULLPATH));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Unlink, sticky_directory)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 01777, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);

	ASSERT_EQ(-1, unlink(FULLPATH));
	ASSERT_EQ(EPERM, errno);
}
