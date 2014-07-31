using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;

public partial class api_SrvNotes : WOApiWebPage
{
    void GetNotes()
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_NoteGetAll";
        sqcmd.Parameters.AddWithValue("@in_GameServerID", web.Param("GameSID"));
        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        DateTime CurUtcDate = Convert.ToDateTime(reader["CurUtcDate"]);
        reader.NextResult();

        StringBuilder xml = new StringBuilder();
        xml.Append("<?xml version=\"1.0\"?>\n");
        xml.Append("<notes>\n");
        while (reader.Read())
        {
            DateTime ExpireUtcDate = Convert.ToDateTime(reader["ExpireUtcDate"]);
            Int64 ExpireMins = Convert.ToInt32((ExpireUtcDate - CurUtcDate).TotalMinutes);
            xml.Append("<note ");
            xml.Append(xml_attr("NoteID", reader["NoteID"]));
            xml.Append(xml_attr("CreateDate", ToUnixTime(Convert.ToDateTime(reader["CreateUtcDate"]))));
            xml.Append(xml_attr("ExpireMins", ExpireMins.ToString()));
            xml.Append(xml_attr("TextFrom", reader["TextFrom"]));
            xml.Append(xml_attr("TextSubj", reader["TextSubj"]));
            xml.Append(xml_attr("GamePos", reader["GamePos"]));
            xml.Append("/>");
        }
        xml.Append("</notes>\n");

        GResponse.Write(xml.ToString());
    }

    void AddNote()
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_NoteAddNew";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", web.CustomerID());
        sqcmd.Parameters.AddWithValue("@in_CharID", web.Param("CharID"));
        sqcmd.Parameters.AddWithValue("@in_GameServerID", web.Param("GameSID"));
        sqcmd.Parameters.AddWithValue("@in_GamePos", web.Param("GamePos"));
        sqcmd.Parameters.AddWithValue("@in_ExpireMins", web.Param("ExpMins"));
        sqcmd.Parameters.AddWithValue("@in_TextFrom", web.Param("TextFrom"));
        sqcmd.Parameters.AddWithValue("@in_TextSubj", web.Param("TextSubj"));
        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        int NoteID = getInt("NoteID");
        Response.Write("WO_0");
        Response.Write(string.Format("{0}", NoteID));
    }

    protected override void Execute()
    {
        string skey1 = web.Param("skey1");
        if (skey1 != SERVER_API_KEY)
            throw new ApiExitException("bad key");

        string func = web.Param("func");
        if (func == "get")
            GetNotes();
        else if (func == "add")
            AddNote();
        else
            throw new ApiExitException("bad func");
    }
}
