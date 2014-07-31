using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;

public partial class api_LeaderboardGet : WOApiWebPage
{
    protected override void Execute()
    {
        //this API is public
        //if (!WoCheckLoginSession())
        //    return;

        string Hardcore = web.Param("Hardcore");
        string Type = web.Param("Type");
        string Page = web.Param("Page");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_LeaderboardGet";
        //sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_Hardcore", Hardcore);
        sqcmd.Parameters.AddWithValue("@in_Type", Type);
        sqcmd.Parameters.AddWithValue("@in_Page", Page);

        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        int StartPos = getInt("StartPos");
        int PageCount = getInt("PageCount");

        // report page of leaderboard
        StringBuilder xml = new StringBuilder();
        xml.Append("<?xml version=\"1.0\"?>\n");
        xml.Append(string.Format("<leaderboard pos=\"{0}\" pc=\"{1}\">", StartPos, PageCount));

        reader.NextResult();
        while (reader.Read())
        {
            xml.Append("<f ");
            xml.Append(xml_attr("gt", reader["Gamertag"]));
            xml.Append(xml_attr("a", reader["Alive"]));
            xml.Append(xml_attr("d", reader["Data"]));
            xml.Append(xml_attr("cid", reader["ClanId"]));
            xml.Append("/>");
        }
        xml.Append("</leaderboard>");

        GResponse.Write(xml.ToString());
    }
}
