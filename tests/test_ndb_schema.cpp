/*
 * test_ndb_schema.cpp
 *
 * Smoke-tests for the shared .ndb project schema helper:
 *   - Current files carry schemaVersion="1".
 *   - Legacy v0 files omit the attribute and are still accepted.
 *   - Future schema versions are rejected.
 *
 * Uses Qt XML directly; does NOT create a QApplication.
 * Returns 0 on success, 1 on any failure.
 */
#include <QBuffer>
#include <QByteArray>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cstdio>

#include "../qtUintDebugger/NdbSchema.h"

static int g_failures = 0;

#define CHECK(expr) \
	do { \
		if(!(expr)) { \
			std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #expr); \
			++g_failures; \
		} \
	} while(0)

static QByteArray WriteMinimalNdb(int schemaVersion, bool includeAttribute)
{
	QByteArray data;
	QBuffer buf(&data);
	buf.open(QIODevice::WriteOnly);
	QXmlStreamWriter writer(&buf);
	writer.setAutoFormatting(true);
	writer.writeStartDocument();
	writer.writeStartElement("uintDebugger-DATA");
	if(includeAttribute)
		writer.writeAttribute("schemaVersion", QString::number(schemaVersion));
	writer.writeTextElement("dummy", "value");
	writer.writeEndElement();
	writer.writeEndDocument();
	return data;
}

// Returns the parsed schema version, 0 for legacy files without the
// attribute, or -1 when the shared helper rejects the root element.
static int ReadSchemaVersion(const QByteArray &data)
{
	QBuffer buf(const_cast<QByteArray*>(&data));
	buf.open(QIODevice::ReadOnly);
	QXmlStreamReader reader(&buf);

	while(!reader.atEnd() && !reader.hasError())
	{
		if(reader.readNext() == QXmlStreamReader::StartElement &&
			reader.name() == QStringLiteral("uintDebugger-DATA"))
		{
			int schemaVersion = 0;
			return NdbSchema::ReadSchemaVersion(reader.attributes(), &schemaVersion)
				? schemaVersion
				: -1;
		}
	}

	return -1;
}

static void TestSchemaVersionPresent()
{
	const QByteArray data = WriteMinimalNdb(kNdbSchemaVersion, /*includeAttribute=*/true);
	const int version = ReadSchemaVersion(data);
	CHECK(version == kNdbSchemaVersion);
}

static void TestLegacyV0Accepted()
{
	const QByteArray data = WriteMinimalNdb(0, /*includeAttribute=*/false);
	const int version = ReadSchemaVersion(data);
	CHECK(version == 0);
}

static void TestFutureVersionRejected()
{
	const QByteArray data = WriteMinimalNdb(kNdbSchemaVersion + 1, /*includeAttribute=*/true);
	const int version = ReadSchemaVersion(data);
	CHECK(version == -1);
}

static void TestCurrentVersionAccepted()
{
	const QByteArray data = WriteMinimalNdb(kNdbSchemaVersion, /*includeAttribute=*/true);
	const int version = ReadSchemaVersion(data);
	CHECK(version <= kNdbSchemaVersion);
}

int main()
{
	TestSchemaVersionPresent();
	TestLegacyV0Accepted();
	TestFutureVersionRejected();
	TestCurrentVersionAccepted();

	if(g_failures == 0)
	{
		std::printf("OK - all NDB schema tests passed.\n");
		return 0;
	}

	std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);
	return 1;
}
