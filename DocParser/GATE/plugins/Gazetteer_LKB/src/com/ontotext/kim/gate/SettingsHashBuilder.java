package com.ontotext.kim.gate;

import java.io.File;
import java.io.IOException;
import java.net.URL;

import org.apache.commons.io.FileUtils;

import com.ontotext.kim.util.StringTransformations;

public class SettingsHashBuilder {

	public int getHash(URL configFile, String query) {
		query = StringTransformations.stripMultiWS(query);
		try {
			String configString = FileUtils.readFileToString(new File(configFile.getFile()));
			configString = StringTransformations.stripMultiWS(configString);
			return (query + ";" + configString).hashCode();
		}
		catch (IOException e) {
			return query.hashCode();
		}
	}
}
