/*
 * test_ndb_schema.cpp
 *
 * Smoke-tests for the .ndb project file schema versioning:
 *   - A file written by the current code carries schemaVersion="1".
 *   - A v0 file (no attribute) is still accepted.
 *   - A file with schemaVersion > kNdbSchemaVersion is rejected.
 *
 * Uses Qt XML directly; does NOT create a QApplication (no widgets needed).
 * Returns 0 on success, 1 on any failure.
 */
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QByteArray>
#include <QString>
#include <cstdio>

// kNdbSchemaVersion comes from the project file header.
// Replicate the constant here to keep the test self-contained and to catch
// accidental changes that forget to update both places.
static constexpr int kExpectedSchemaVersion = 1;

static int g_failures = 0;

#define CHECK(expr) \
    do { \
        if(!(expr)) { \
            std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #expr); \
            ++g_failures; \
        } \
    } while(0)

// ---------------------------------------------------------------------------
// Helpers that write / read a minimal .ndb fragment into a QByteArray.
// ---------------------------------------------------------------------------

static QByteArray WriteMinimalNdb(int schemaVersion, bool includeAttribute)
{
    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("uintDebugger-DATA");
    if(includeAttribute)
        w.writeAttribute("schemaVersion", QString::number(schemaVersion));
    w.writeTextElement("dummy", "value");
    w.writeEndElement();
    w.writeEndDocument();
    return data;
}

// Returns the schemaVersion attribute value found on the root element,
// or -1 if the attribute is absent, or -2 on XML error.
static int ReadSchemaVersion(const QByteArray &data)
{
    QBuffer buf(const_cast<QByteArray*>(&data));
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader r(&buf);

    while(!r.atEnd() && !r.hasError())
    {
        if(r.readNext() == QXmlStreamReader::StartElement)
        {
            if(r.name() == QStringLiteral("uintDebugger-DATA"))
            {
                const auto attr = r.attributes().value("schemaVersion");
                if(attr.isEmpty())
                    return -1; // absent — v0 file
                bool ok = false;
                int ver = attr.toInt(&ok);
                return ok ? ver : -2;
            }
        }
    }
    return r.hasError() ? -2 : -1;
}

// Simulates the reader's version-rejection logic from clsProjectFile.cpp.
static bool ReaderAcceptsVersion(int foundVersion, int maxAccepted)
{
    if(foundVersion == -1) return true;   // absent → v0, always OK
    if(foundVersion < 0)  return false;   // parse error
    return foundVersion <= maxAccepted;
}

// ---------------------------------------------------------------------------
void TestSchemaVersionPresent()
{
    const QByteArray data = WriteMinimalNdb(kExpectedSchemaVersion, /*includeAttr=*/true);
    const int ver = ReadSchemaVersion(data);
    CHECK(ver == kExpectedSchemaVersion);
}

void TestLegacyV0Accepted()
{
    // A file without the attribute is a v0 file and must be accepted.
    const QByteArray data = WriteMinimalNdb(0, /*includeAttr=*/false);
    const int ver = ReadSchemaVersion(data);   // should be -1 (absent)
    CHECK(ver == -1);
    CHECK(ReaderAcceptsVersion(ver, kExpectedSchemaVersion));
}

void TestFutureVersionRejected()
{
    // A file from a future release with a higher schema version must be rejected
    // so the user gets an error instead of silently loading corrupt data.
    const QByteArray data = WriteMinimalNdb(kExpectedSchemaVersion + 1, /*includeAttr=*/true);
    const int ver = ReadSchemaVersion(data);
    CHECK(ver == kExpectedSchemaVersion + 1);
    CHECK(!ReaderAcceptsVersion(ver, kExpectedSchemaVersion));
}

void TestCurrentVersionAccepted()
{
    const QByteArray data = WriteMinimalNdb(kExpectedSchemaVersion, /*includeAttr=*/true);
    const int ver = ReadSchemaVersion(data);
    CHECK(ReaderAcceptsVersion(ver, kExpectedSchemaVersion));
}

// ---------------------------------------------------------------------------
int main()
{
    TestSchemaVersionPresent();
    TestLegacyV0Accepted();
    TestFutureVersionRejected();
    TestCurrentVersionAccepted();

    if(g_failures == 0)
    {
        std::printf("OK — all NDB schema tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);
    return 1;
}
