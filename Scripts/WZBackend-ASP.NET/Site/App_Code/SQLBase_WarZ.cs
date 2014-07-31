using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Web;
using System.Data.SqlClient;
using System.Diagnostics;
using System.Configuration;
using System.Web.Configuration;

/// <summary>
/// Summary description for SQLBase
/// </summary>
public class SQLBase
{
    SqlConnection conn_ = null;

    public SQLBase()
    {
    }

    ~SQLBase()
    {
        Disconnect();
    }

    public bool Connect()
    {
        try
        {
            SqlConnection c = new SqlConnection(
                WebConfigurationManager.ConnectionStrings["dbConnectionString"].ToString());
            c.Open();

            conn_ = c;
        }
        catch (Exception)
        {
            throw new ApiExitException("SQL Connect failed");
            //return false;
        }

        return true;
    }

    public void Disconnect()
    {
        if (conn_ == null)
            return;

        try
        {
            conn_.Close();
        }
        catch { }
        conn_ = null;
    }

    void DumpSqlCommand(SqlCommand sqcmd)
    {
        Debug.WriteLine("CMD: " + sqcmd.CommandText);

        foreach (SqlParameter p in sqcmd.Parameters)
        {
            Debug.WriteLine(string.Format("{0}={1} ({2})",
                p.ParameterName, p.SqlValue, p.SqlDbType.ToString()));
        }

        return;
    }

    public SqlDataReader Select(SqlCommand sqcmd)
    {
        SqlDataReader reader = null;
        try
        {
            sqcmd.Connection = conn_;
            //DumpSqlCommand(sqcmd);
            reader = sqcmd.ExecuteReader();
            return reader;
        }
        catch (Exception e)
        {
#if true
            //@DEBUG
            throw new ApiExitException("SQL Select Failed: " + e.Message);
#else
            throw new ApiExitException("SQL");
#endif
            //return false;
        }
    }

    public SqlTransaction BeginTransaction()
    {
        return conn_.BeginTransaction();
    }
}
